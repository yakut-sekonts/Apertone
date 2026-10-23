#include "apps/shared/PhotoDecoder.h"
#include "core/export/LinearOutput.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/ImageAlignment.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>

#ifndef LUMA_UX_MERGE_PROBE
#error "Merge observation is restricted to the UX executable"
#endif
namespace ux {
// Copy the real handle on the GUI thread. Cancellation clears aiBusy before the
// worker exits; a terminal handle is required before inspecting cleanup.
class MergeJobProbe final {
  public:
    static luma::JobHandle job(const EditorController &editor);
};
luma::JobHandle MergeJobProbe::job(const EditorController &editor) {
    return editor.aiJob_;
}
namespace {
struct WriteHold {
    SqliteFault &fault;
    explicit WriteHold(SqliteFault &value) : fault(value) {
        fault.holdWrites(true);
    }
    ~WriteHold() {
        fault.holdWrites(false);
    }
    void release() {
        fault.holdWrites(false);
    }
};
void idle(Context &c) {
    c.driver->wait(
        [&] { return !c.library.working() && !c.library.loading() && !c.library.selecting(); },
        "Merge Library settled");
}
std::vector<luma::AssetRecord> importSources(Context &c, bool panorama) {
    auto &d = *c.driver;
    QList<QUrl> sources;
    for (const auto &name :
         QDir(c.fixture).entryList({panorama ? "*.png" : "*.dng"}, QDir::Files, QDir::Name))
        sources.append(QUrl::fromLocalFile(c.fixture + '/' + name));
    require(sources.size() == (panorama ? 2 : 3), "Wrong owned merge fixture count");
    require(d.object("library.importDialog")->setProperty("sources", QVariant::fromValue(sources)),
            "Import dialog refused merge source URLs");
    d.click("library.importButton");
    d.wait([&] { return d.object("library.importDialog")->property("opened").toBool(); },
           "Merge import dialog opened");
    d.click("library.importStart");
    d.wait([&] { return !c.library.working() && c.library.count() == sources.size(); },
           "Owned merge originals imported");
    idle(c);
    require(c.library.error().isEmpty(), c.library.error());
    std::vector<luma::AssetRecord> result;
    luma::Catalog catalog(c.catalogFile.toStdString());
    for (const auto &id : catalog.libraryIDs({})) {
        const auto asset = catalog.assetByID(id);
        require(asset.has_value(), "Imported merge source disappeared");
        result.push_back(*asset);
    }
    d.click(c.card(0));
    d.click(c.card(static_cast<int>(sources.size()) - 1), Qt::ShiftModifier);
    idle(c);
    require(c.library.selectedCount() == sources.size(), "Merge multi-selection failed");
    return result;
}
// Independent bounded TIFF reader: inspect the stored Float DNG, not a preview
// or production decoder that could normalize/clamp the values under test.
struct FloatDng {
    QByteArray bytes;
    int width = 0, height = 0;
    double maximum = 0;
    std::vector<float> pixels;
    explicit FloatDng(const std::filesystem::path &path) {
        QFile file(QString::fromStdString(path.string()));
        require(file.open(QIODevice::ReadOnly), "Cannot inspect derived DNG");
        bytes = file.readAll();
        auto u16 = [&](quint64 offset) {
            require(offset + 2 <= static_cast<quint64>(bytes.size()), "Truncated TIFF short");
            return qFromLittleEndian<quint16>(bytes.constData() + offset);
        };
        auto u32 = [&](quint64 offset) {
            require(offset + 4 <= static_cast<quint64>(bytes.size()), "Truncated TIFF long");
            return qFromLittleEndian<quint32>(bytes.constData() + offset);
        };
        require(bytes.startsWith("II") && u16(2) == 42, "Derived output is not little-endian TIFF");
        auto directory = [&](quint64 offset) {
            std::map<unsigned, quint64> tags;
            const auto count = u16(offset);
            require(count < 256, "Unbounded derived TIFF directory");
            for (unsigned i = 0; i < count; ++i) {
                const auto entry = offset + 2 + 12 * i;
                (void)u32(entry + 8);
                require(tags.emplace(u16(entry), entry).second, "Duplicate TIFF field");
            }
            return tags;
        };
        auto fields = directory(u32(4));
        require(fields.contains(330), "Derived output has no LinearRaw SubIFD");
        fields = directory(u32(fields.at(330) + 8));
        auto values = [&](unsigned tag, unsigned type, unsigned count) {
            require(fields.contains(tag), "Missing derived TIFF field");
            const auto entry = fields.at(tag);
            require(u16(entry + 2) == type && u32(entry + 4) == count,
                    "Incorrect derived TIFF field representation");
            const unsigned size = type == 3 ? 2 : 4;
            const quint64 start = size * count > 4 ? u32(entry + 8) : entry + 8;
            std::vector<quint32> result;
            for (unsigned i = 0; i < count; ++i)
                result.push_back(type == 3 ? u16(start + i * size) : u32(start + i * size));
            return result;
        };
        require(values(258, 3, 3) == std::vector<quint32>{32, 32, 32} &&
                    values(339, 3, 1).front() == 3 && values(262, 3, 1).front() == 34892 &&
                    values(259, 3, 1).front() == 1 && values(50717, 4, 1).front() == 1,
                "Derived DNG lost floating-point LinearRaw representation");
        width = static_cast<int>(values(256, 4, 1).front());
        height = static_cast<int>(values(257, 4, 1).front());
        require(width > 0 && width <= 1024 && height > 0 && height <= 1024,
                "Unexpected merge fixture dimensions");
        const quint64 start = values(273, 4, 1).front();
        const quint64 length = values(279, 4, 1).front();
        require(length == static_cast<quint64>(width) * height * 12 &&
                    start + length <= static_cast<quint64>(bytes.size()),
                "Truncated or incorrectly sized Float DNG pixels");
        for (quint64 i = 0; i < length; i += 4) {
            const float value = std::bit_cast<float>(u32(start + i));
            require(std::isfinite(value) && value >= 0, "Invalid radiometric merge sample");
            maximum = std::max(maximum, static_cast<double>(value));
            pixels.push_back(value);
        }
    }
};
void unchanged(Context &c, const std::vector<luma::AssetRecord> &sources) {
    luma::Catalog catalog(c.catalogFile.toStdString());
    for (const auto &before : sources) {
        const auto after = catalog.assetByID(before.id);
        require(after && after->path == before.path && after->checksum == before.checksum &&
                    after->edits == before.edits &&
                    luma::macos::sha256File(before.path) == before.checksum,
                "Merge changed a source file or its Develop edits");
    }
}
} // namespace
bool mergeWorkflowScenario(Context &c, const QString &id) {
    const QStringList supported{
        "UX-P08-MRG-001", "UX-P08-MRG-003", "UX-P08-MRG-004", "UX-P08-MRG-005", "UX-P08-MRG-006",
        "UX-P08-MRG-009", "UX-P08-MRG-010", "UX-P08-HDR-001", "UX-P08-HDR-009", "UX-P08-PAN-001",
        "UX-P08-PAN-002", "UX-P08-FLT-002", "UX-P08-PAN-004", "UX-P08-ALG-002", "UX-P08-ALG-001"};
    if (!supported.contains(id))
        return false;
    const bool reject = id == "UX-P08-PAN-004" || id == "UX-P08-ALG-002";
    const bool panorama = id.startsWith("UX-P08-PAN-") || id == "UX-P08-ALG-001" || reject;
    const bool cancel = id == "UX-P08-MRG-004" || id == "UX-P08-MRG-005";
    auto &d = *c.driver;
    const auto sources = importSources(c, panorama);
    if (id == "UX-P08-ALG-001") {
        // Exercise the actual platform API separately, then verify its real UI
        // consumer below. These observations do not replace the merge worker.
        QImage reference(c.fixture + "/Frame01.png"), floating(c.fixture + "/Frame02.png");
        reference = reference.convertToFormat(QImage::Format_RGBA8888);
        floating = floating.convertToFormat(QImage::Format_RGBA8888);
        require(!reference.isNull() && !floating.isNull(), "Missing alignment ground truth");
        const auto transform = luma::macos::alignImages(
            {reference.width(),
             reference.height(),
             {reference.constBits(), static_cast<std::size_t>(reference.sizeInBytes())}},
            {floating.width(),
             floating.height(),
             {floating.constBits(), static_cast<std::size_t>(floating.sizeInBytes())}},
            {}, luma::macos::AlignmentMode::panorama);
        QJsonArray coefficients;
        for (const auto value : transform.values) {
            require(std::isfinite(value) && std::abs(value) <= 1e6,
                    "Non-finite or excessive alignment coefficient");
            coefficients.append(value);
        }
        const auto inverse = transform.inverse();
        double maximumError = 0;
        for (int y = 0; y <= 10; ++y)
            for (int x = 0; x <= 10; ++x) {
                const double u = x / 10., v = y / 10.;
                const auto point = transform.point(u, v);
                const auto restored = inverse.point(point[0], point[1]);
                require(std::isfinite(point[0]) && std::isfinite(point[1]) &&
                            std::abs(restored[0] - u) < 1e-9 && std::abs(restored[1] - v) < 1e-9,
                        "Alignment grid is non-finite or non-invertible");
                maximumError = std::max({maximumError, std::abs(point[0] - u - .25) * 256,
                                         std::abs(point[1] - v) * 192});
            }
        d.log({{"action", "platform_alignment_bounds"},
               {"coefficients", coefficients},
               {"maximum_ground_truth_pixel_error", maximumError},
               {"pixel_budget", 1.0}});
        require(maximumError <= 1, "Alignment exceeds one pixel of the known translation");
    }
    d.click("library.photoActions");
    d.click(panorama ? "library.panorama" : "library.hdr");
    const QString dialog = panorama ? "merge.panorama" : "merge.hdr";
    d.wait([&] { return d.object(dialog)->property("opened").toBool(); },
           "Selected merge dialog opened");
    if (panorama)
        d.choose("merge.projection", 0);
    // Real catalog writes are held only at the storage boundary, after actual
    // decode, Vision alignment, merge and Float DNG encoding have completed.
    WriteHold hold(c.storageFault);
    d.click(dialog + ".create");
    const auto job = MergeJobProbe::job(c.editor);
    d.wait([&] { return c.storageFault.writeBlocked() || !c.editor.aiBusy(); },
           "Actual merge reached durable catalog staging", 60000);
    if (reject) {
        require(!c.storageFault.writeBlocked(), "Unrelated originals produced a staged panorama");
        hold.release();
        d.wait(
            [&] {
                return job.state() != luma::JobState::queued &&
                       job.state() != luma::JobState::running;
            },
            "Refused alignment worker terminated");
        d.frame();
        luma::Catalog catalog(c.catalogFile.toStdString());
        require(!c.editor.aiBusy() && c.editor.error().contains("align") &&
                    d.item("main.editorError")->isVisible() &&
                    d.object("main.editorError")->property("text").toString() == c.editor.error() &&
                    catalog.libraryCount() == static_cast<std::int64_t>(sources.size()) &&
                    catalog.pendingDerived().empty() &&
                    QDir(c.catalogFile + ".luma-derived")
                        .entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)
                        .empty(),
                "Alignment refusal did not explain failure or preserve clean storage");
        d.log({{"action", "unrelated_panorama_refused"}, {"visible_error", c.editor.error()}});
        d.screenshot(c.root + "/alignment-refused.png");
        unchanged(c, sources);
        // The runner requires a recovered, error-free final UI. Open an intact
        // source through its card instead of suppressing the expected error.
        d.doubleClick(c.card(0));
        c.loaded();
        c.saved();
        return true;
    }
    require(c.storageFault.writeBlocked(), "Merge failed before staging: " + c.editor.error());
    d.frame();
    require(c.editor.aiBusy() && d.item("merge.progress")->isVisible() &&
                d.item("merge.cancel")->isEnabled() &&
                d.object("merge.operation")->property("text").toString() ==
                    (panorama ? "Panorama merge" : "HDR merge") &&
                d.object("merge.progress")->property("value").toDouble() > 0 &&
                d.object("merge.progress")->property("value").toDouble() <= 1,
            "Active merge phase/progress/Cancel is not visible");
    require(d.item("merge.phase")->isVisible() &&
                d.object("merge.phase")->property("text").toString() == "Writing Float DNG…",
            "Merge did not expose its actual encoding phase");
    d.screenshot(c.root + "/merge-progress.png");
    if (cancel)
        d.click("merge.cancel");
    hold.release();
    d.wait(
        [&] {
            return job.state() != luma::JobState::queued && job.state() != luma::JobState::running;
        },
        "Actual merge worker reached its terminal state");
    if (cancel) {
        require(!c.editor.aiBusy() && c.editor.error().isEmpty(),
                "Cancel left merge UI busy/error");
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto folder = c.catalogFile + ".luma-derived";
        const auto files =
            QDir(folder).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        d.log({{"action", "merge_cancel_before_install"},
               {"files", QJsonArray::fromStringList(files)},
               {"catalog_count", static_cast<qint64>(catalog.libraryCount())},
               {"pending_records", static_cast<qint64>(catalog.pendingDerived().size())}});
        require(catalog.libraryCount() == static_cast<std::int64_t>(sources.size()) &&
                    catalog.pendingDerived().empty() && files.empty(),
                "Cancellation before install left a derived file, journal or catalog result");
    } else {
        d.wait([&] { return !c.editor.aiBusy() && c.editor.hasPhoto(); },
               "Merged Float DNG opened in Develop");
        c.loaded();
        c.saved();
        idle(c);
        require(c.editor.error().isEmpty(), c.editor.error());
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto result = catalog.assetByID(c.editor.photoID().toStdString());
        require(result && c.library.count() == static_cast<int>(sources.size() + 1) &&
                    catalog.libraryCount() == static_cast<std::int64_t>(sources.size() + 1) &&
                    catalog.pendingDerived().empty(),
                "Successful merge did not enter Library exactly once");
        const FloatDng output(result->path);
        for (const auto &source : sources)
            require(
                output.bytes.contains(QByteArray::fromStdString(source.id + ':' + source.checksum)),
                "Derived provenance omitted a source identity/checksum");
        if (!panorama)
            require(output.width == 256 && output.height == 192 && output.maximum > 1.1,
                    "HDR did not preserve source geometry and values above SDR white");
        else if (id.endsWith("001"))
            require(std::abs(output.width - 320) <= 5 && std::abs(output.height - 192) <= 5,
                    "Horizontal panorama lost the known 64-pixel overlap displacement");
        else
            require(std::abs(output.width - 256) <= 5 && std::abs(output.height - 240) <= 5,
                    "Vertical panorama lost the known 48-pixel overlap displacement");
        if (panorama) {
            const QImage first(c.fixture + "/Frame01.png"), second(c.fixture + "/Frame02.png");
            require(!first.isNull() && !second.isNull(), "Cannot read panorama pixel ground truth");
            double maximumError = 0;
            for (int y = 10; y < output.height - 10; y += 5)
                for (int x = 10; x < output.width - 10; x += 5) {
                    const auto color = x < 256 && y < 192
                                           ? first.pixelColor(x, y)
                                           : second.pixelColor(x - (id.endsWith("001") ? 64 : 0),
                                                               y - (id.endsWith("002") ? 48 : 0));
                    const double encoded = color.red() / 255.0;
                    const double expected = encoded <= .04045
                                                ? encoded / 12.92
                                                : std::pow((encoded + .055) / 1.055, 2.4);
                    const auto offset = (static_cast<std::size_t>(y) * output.width + x) * 3;
                    for (int channel = 0; channel < 3; ++channel)
                        maximumError = std::max(
                            maximumError, std::abs(output.pixels[offset + channel] - expected));
                }
            d.log({{"action", "panorama_known_world_pixels"},
                   {"maximum_error", maximumError},
                   {"linear_rgb_budget", .001}});
            require(maximumError <= .001,
                    "Panorama pixels differ from the independently reconstructed source scene");
        }
        d.log({{"action", "merged_float_dng"},
               {"width", output.width},
               {"height", output.height},
               {"maximum_linear_value", output.maximum},
               {"source_count", static_cast<int>(sources.size())}});
        const auto original = c.editor.currentSettings();
        const auto before = c.editor.previewUrl();
        c.reveal("develop.exposureSlider");
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {1., .5});
        c.saved();
        d.wait([&] { return c.editor.previewUrl() != before && !c.editor.histogramPending(); },
               "Derived-only exposure preview completed");
        require(c.editor.currentSettings().exposure == 5 && c.editor.canUndo(),
                "Float DNG could not be independently edited");
        const auto decoded = luma::PhotoDecoder().decode(result->path);
        if (!panorama)
            require(*std::max_element(decoded.image.rgb.begin(), decoded.image.rgb.end()) > 1.1F,
                    "Reopening Float DNG clipped its HDR samples");
        auto verifyPixels = [&] {
            const auto document = c.editor.currentSettings();
            document.validate();
            const auto linear = luma::renderLinearOutput(decoded, document);
            require(std::all_of(linear.rgba.begin(), linear.rgba.end(),
                                [](float value) { return std::isfinite(value); }),
                    "Develop adjustment produced non-finite Float DNG output");
            const auto expected = fixtureReference(c, document, c.editor.sourcePath());
            d.wait(
                [&] {
                    return previewPixelsAgree(c.provider->requestImage({}, nullptr, {}), expected,
                                              c.editor.metalPreview());
                },
                "Edited Float DNG preview matches saved Develop settings");
        };
        verifyPixels();
        if (id == "UX-P08-FLT-002") {
            for (const auto *parameter : {"contrast", "highlights", "shadows", "saturation"}) {
                const auto control = QStringLiteral("develop.") + parameter + "Slider";
                c.reveal(control);
                d.drag(control, d.sliderThumb(control), {.75, .5});
                c.saved();
                require(std::abs(c.editor.currentSettings().parameter(parameter)) > 1,
                        "Float DNG slider did not change its parameter");
                verifyPixels();
                d.click("main.undoButton");
                c.saved();
            }
        }
        d.click("main.undoButton");
        c.saved();
        require(c.editor.currentSettings() == original && c.editor.canRedo() &&
                    luma::macos::sha256File(result->path) == result->checksum,
                "Derived Undo or file preservation failed");
    }
    unchanged(c, sources);
    return true;
}
bool mergeOptionsScenario(Context &c, const QString &id) {
    if (id != "UX-P08-HDR-006" && id != "UX-P08-HDR-007" && id != "UX-P08-DGH-001")
        return false;
    const bool alignment = id == "UX-P08-HDR-006";
    auto &d = *c.driver;
    const auto sources = importSources(c, false);
    const auto middle = std::find_if(sources.begin(), sources.end(), [](const auto &source) {
        return source.path.filename() == "Frame02.dng";
    });
    require(middle != sources.end(), "Missing reference exposure ground truth");
    luma::EditDocument neutral;
    neutral.enabledCorrections = 0;
    const auto reference =
        luma::renderLinearOutput(luma::PhotoDecoder().decode(middle->path), neutral);
    std::vector<std::vector<float>> pixels;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) {
            d.click("main.libraryButton");
            idle(c);
            d.click("library.clearSelection");
            for (std::size_t i = 0; i < sources.size(); ++i)
                d.click("library.card." + QString::fromStdString(sources[i].id),
                        i ? Qt::ControlModifier : Qt::NoModifier);
            idle(c);
        }
        require(c.library.selectedCount() == 3, "HDR option run lost original multi-selection");
        d.click("library.photoActions");
        d.click("library.hdr");
        d.wait([&] { return d.object("merge.hdr")->property("opened").toBool(); },
               "HDR options dialog opened");
        const bool enabled = alignment && pass == 1;
        if (d.object("merge.alignment")->property("checked").toBool() != enabled)
            d.click("merge.alignment");
        d.drag("merge.deghost", d.sliderThumb("merge.deghost"),
               {alignment || pass == 0 ? 0. : 1., .5});
        const double strength = alignment || pass == 0 ? 0 : 100;
        require(d.object("merge.alignment")->property("checked").toBool() == enabled &&
                    d.object("merge.deghost")->property("value").toDouble() == strength,
                "Actual HDR controls did not retain the requested options");
        const auto previous = c.editor.photoID();
        d.click("merge.hdr.create");
        const auto job = MergeJobProbe::job(c.editor);
        d.wait([&] { return !c.editor.aiBusy(); }, "HDR options worker completed", 60000);
        require(c.editor.error().isEmpty(), c.editor.error());
        d.wait(
            [&] {
                return c.editor.photoID() != previous && job.state() != luma::JobState::running;
            },
            "HDR option result became the active derived asset");
        c.loaded();
        c.saved();
        idle(c);
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto result = catalog.activeAsset();
        require(result && catalog.libraryCount() == 4 + pass && catalog.pendingDerived().empty(),
                "HDR option run did not publish exactly one distinct result");
        const FloatDng output(result->path);
        require(
            output.width == reference.width && output.height == reference.height &&
                output.bytes.contains("luma:AlignFrames=\"" + QByteArray::number(enabled) + "\"") &&
                output.bytes.contains("luma:Deghost=\"" + QByteArray::number(strength / 100) +
                                      "\""),
            "HDR output geometry/provenance differs from the chosen options");
        const auto expected = fixtureReference(c);
        d.wait(
            [&] {
                return previewPixelsAgree(c.provider->requestImage({}, nullptr, {}), expected,
                                          c.editor.metalPreview());
            },
            "HDR option result preview matches its actual Float DNG");
        pixels.push_back(output.pixels);
        unchanged(c, sources);
    }
    std::array<double, 2> errors{};
    double difference = 0;
    std::size_t samples = 0;
    for (int y = 20; y < 172; ++y)
        for (int x = 20; x < 200; ++x) {
            if (!alignment &&
                !(y >= 62 && y < 90 && ((x >= 52 && x < 76) || (x >= 116 && x < 140))))
                continue;
            const auto offset = static_cast<std::size_t>(y) * reference.width + x;
            for (int channel = 0; channel < 3; ++channel) {
                const double expected = reference.rgba[offset * 4 + channel];
                for (int pass = 0; pass < 2; ++pass) {
                    const double error = pixels[pass][offset * 3 + channel] - expected;
                    errors[pass] += error * error;
                }
                const double delta =
                    pixels[0][offset * 3 + channel] - pixels[1][offset * 3 + channel];
                difference += delta * delta;
                ++samples;
            }
        }
    for (auto &error : errors)
        error /= samples;
    difference /= samples;
    d.log({{"action", alignment ? "hdr_alignment_ground_truth" : "hdr_motion_ground_truth"},
           {"disabled_mse", errors[0]},
           {"enabled_mse", errors[1]},
           {"option_difference_mse", difference},
           {"maximum_error_ratio", .6}});
    require(errors[0] > 1e-6 && difference > 1e-6 && errors[1] < errors[0] * .6,
            "HDR option did not improve the known shifted/moving-object scene");
    return true;
}
bool mergeRestartScenario(Context &c, const QString &id, const QString &stage) {
    if (id != "UX-P08-PAN-012" && id != "UX-P08-FLT-001")
        return false;
    auto &d = *c.driver;
    if (stage == "prepare") {
        require(
            mergeWorkflowScenario(c, id == "UX-P08-PAN-012" ? "UX-P08-PAN-001" : "UX-P08-HDR-001"),
            "Merge restart requires a complete successful creation workflow");
        c.reveal("develop.exposureSlider");
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.3, .5});
        c.saved();
        const auto document = c.editor.currentSettings();
        require(document.exposure < -1,
                "Restart fixture needs a range-revealing negative exposure");
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto active = catalog.activeAsset();
        require(active.has_value(), "Derived restart asset is missing");
        luma::Catalog expected((c.stateRoot + "/expected.sqlite").toStdString());
        const auto stored = expected.addAndSelect(active->path, active->checksum, active->metadata);
        expected.saveEdits(stored.id, document);
        QJsonArray sources;
        for (const auto &sourceID : catalog.libraryIDs({})) {
            const auto source = catalog.assetByID(sourceID);
            require(source.has_value(), "Restart source oracle disappeared");
            sources.append(QJsonObject{{"id", QString::fromStdString(sourceID)},
                                       {"path", QString::fromStdString(source->path.string())},
                                       {"checksum", QString::fromStdString(source->checksum)}});
        }
        writeJson(
            c.stateRoot + "/expected-merge.json",
            {{"id", c.editor.photoID()}, {"path", c.editor.sourcePath()}, {"sources", sources}});
        d.quitShortcut();
        d.wait([&] { return !c.window->isVisible() && !c.editor.saving() && !c.editor.busy(); },
               "Derived session closed before fresh process restart");
        c.checkData();
        return true;
    }
    require(stage == "verify", "Invalid merge restart stage");
    QFile file(c.stateRoot + "/expected-merge.json");
    require(file.open(QIODevice::ReadOnly), "Merge restart oracle is missing");
    const auto expected = QJsonDocument::fromJson(file.readAll()).object();
    const auto saved =
        luma::Catalog((c.stateRoot + "/expected.sqlite").toStdString()).activeAsset();
    require(saved.has_value(), "Complete expected derived edit document is missing");
    c.editor.restore();
    c.loaded();
    c.saved();
    require(c.editor.photoID() == expected["id"].toString() &&
                c.editor.sourcePath() == expected["path"].toString() &&
                c.editor.currentSettings() == saved->edits && !c.editor.canUndo() &&
                !c.editor.canRedo(),
            "Restart lost derived identity/edits or restored session-local Undo");
    luma::Catalog catalog(c.catalogFile.toStdString());
    const auto sources = expected["sources"].toArray();
    require(catalog.libraryCount() == sources.size() && catalog.pendingDerived().empty(),
            "Restart duplicated a derived result or left its registration pending");
    for (const auto &value : sources) {
        const auto source = value.toObject();
        const auto current = catalog.assetByID(source["id"].toString().toStdString());
        require(current && QString::fromStdString(current->path.string()) == source["path"] &&
                    QString::fromStdString(current->checksum) == source["checksum"] &&
                    luma::macos::sha256File(current->path) == current->checksum,
                "Restart changed an original or converted a derived Float DNG");
    }
    const FloatDng stored(saved->path);
    const auto decoded = luma::PhotoDecoder().decode(saved->path);
    if (id == "UX-P08-FLT-001")
        require(stored.maximum > 1.1 &&
                    *std::max_element(decoded.image.rgb.begin(), decoded.image.rgb.end()) > 1.1F,
                "Fresh process clipped the stored/decoded HDR dynamic range");
    const auto reference = fixtureReference(c, saved->edits, c.editor.sourcePath());
    d.wait(
        [&] {
            return previewPixelsAgree(c.provider->requestImage({}, nullptr, {}), reference,
                                      c.editor.metalPreview());
        },
        "Fresh process preview shows the complete saved Float DNG document");
    c.reveal("develop.contrastSlider");
    d.drag("develop.contrastSlider", d.sliderThumb("develop.contrastSlider"), {.75, .5});
    c.saved();
    require(c.editor.currentSettings().contrast > 1 && c.editor.canUndo(),
            "Restored derived asset is not editable");
    d.click("main.undoButton");
    c.saved();
    require(c.editor.currentSettings() == saved->edits && c.editor.canRedo() &&
                luma::macos::sha256File(saved->path) == saved->checksum,
            "Post-restart Undo changed the saved derived document or file");
    return true;
}
} // namespace ux
