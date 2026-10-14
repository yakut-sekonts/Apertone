#include "apps/shared/BuiltinPresets.h"
#include "core/render/AdvancedColor.h"
#include "core/render/CpuRenderer.h"
#include "core/render/DevelopPipeline.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSet>
#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read filter fixture/reference.");
    return file.readAll();
}
luma::FloatImage linear(const QImage &image) {
    luma::FloatImage result(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            const auto offset = (static_cast<std::size_t>(y) * image.width() + x) * 3;
            result.rgb[offset] = static_cast<float>(luma::decodeSRGB(color.redF()));
            result.rgb[offset + 1] = static_cast<float>(luma::decodeSRGB(color.greenF()));
            result.rgb[offset + 2] = static_cast<float>(luma::decodeSRGB(color.blueF()));
        }
    return result;
}
QImage image(const luma::DisplayImage &pixels) {
    return QImage(pixels.rgba.data(), pixels.width, pixels.height,
                  static_cast<qsizetype>(pixels.width) * 4, QImage::Format_RGBA8888)
        .copy();
}
QJsonArray metrics(const luma::DisplayImage &pixels, const luma::DisplayImage &baseline) {
    std::array<double, 5> sums{};
    const auto count = luma::pixelCount(pixels.width, pixels.height);
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            const double value = pixels.rgba[i * 4 + c];
            sums[c] += value;
            sums[3] += std::abs(value - baseline.rgba[i * 4 + c]) / 3;
            if (value == 0 || value == 255)
                sums[4] += 100.0 / 3;
        }
        require(pixels.rgba[i * 4 + 3] == 255, "A color look changed pixel coverage.");
    }
    QJsonArray result;
    for (double value : sums)
        result.append(value / static_cast<double>(count));
    // Spatial RGB means catch localized changes that whole-image averages miss.
    for (int tileY = 0; tileY < 4; ++tileY)
        for (int tileX = 0; tileX < 4; ++tileX) {
            std::array<double, 3> region{};
            std::size_t samples = 0;
            for (int y = tileY * pixels.height / 4; y < (tileY + 1) * pixels.height / 4; ++y)
                for (int x = tileX * pixels.width / 4; x < (tileX + 1) * pixels.width / 4; ++x) {
                    const auto offset = (static_cast<std::size_t>(y) * pixels.width + x) * 4;
                    for (std::size_t c = 0; c < 3; ++c)
                        region[c] += pixels.rgba[offset + c];
                    ++samples;
                }
            for (double value : region)
                result.append(value / static_cast<double>(samples));
        }
    return result;
}
void documentChecks() {
    QSet<QString> ids;
    luma::EditDocument current;
    current.setParameters({{"exposure", 1.25},
                           {"temperature", 5100},
                           {"tint", 12},
                           {"sharpen.amount", 80},
                           {"noise.color", 65},
                           {"crop.x", 0.1},
                           {"crop.width", 0.8},
                           {"transform.rotate", 8}});
    current.customWhiteBalance = true;
    current.enabledCorrections = 65536 | 2 | 1 | 2048;
    for (const auto &entry : luma::builtinPresets()) {
        require(!ids.contains(entry.id) && luma::builtinPresetID(entry.id), "Duplicate filter ID.");
        ids.insert(entry.id);
        entry.preset.validate();
        require(entry.preset.groups == 12, "A filter unexpectedly includes excluded groups.");
        const auto roundtrip = luma::decodePreset(luma::encodePreset(entry.preset));
        require(roundtrip.settings == entry.preset.settings, "Filter JSON is not reproducible.");
        const auto applied = luma::mergeSettings(current, entry.preset.settings, 12);
        require(luma::mergeSettings(applied, entry.preset.settings, 12) == applied,
                "Repeated filter application accumulates adjustments.");
        require(luma::mergeSettings(applied, current, 12) == current,
                "Filter overwrote an excluded field/enable state.");
        const luma::DevelopPipeline pipeline(entry.preset.settings);
        for (const auto &pixel :
             {std::array<float, 3>{0, 0, 0}, {1, 1, 1}, {4, -0.2F, 0.6F}, {0.18F, 0.18F, 0.18F}})
            for (float value : pipeline.pixel(pixel))
                require(std::isfinite(value), "Filter produced non-finite HDR output.");
        for (const auto &points : entry.preset.settings.curves) {
            const luma::ToneCurve curve(points);
            double previous = curve.value(0);
            for (int step = 1; step <= 1000; ++step) {
                const double value = curve.value(step / 1000.0);
                require(value >= previous - 1e-12 && value >= 0 && value <= 1,
                        "Authored curve inverted tones or exceeded output endpoints.");
                previous = value;
            }
        }
    }
    require(ids.size() == 8, "Expected eight built-in filters.");
}
} // namespace
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    try {
        require(argc == 3, "Expected fixture and QA output directories.");
        documentChecks();
        const QString fixtures = QString::fromLocal8Bit(argv[1]),
                      output = QString::fromLocal8Bit(argv[2]);
        require(QDir().mkpath(output), "Cannot create QA output directory.");
        const auto manifest = QJsonDocument::fromJson(read(fixtures + "/manifest.json")).array();
        require(manifest.size() == 3, "Representative photo manifest is incomplete.");
        QJsonObject report;
        constexpr int cellWidth = 192, cellHeight = 218;
        QImage sheet(cellWidth * 9, cellHeight * 3, QImage::Format_RGB32);
        sheet.fill(QColor("#191c22"));
        QPainter painter(&sheet);
        painter.setPen(Qt::white);
        int row = 0;
        for (const auto &value : manifest) {
            const auto item = value.toObject();
            const auto name = item["file"].toString();
            const auto bytes = read(fixtures + "/" + name);
            require(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() ==
                        item["sha256"].toString().toLatin1(),
                    "Licensed photo fixture checksum changed.");
            const auto source = QImage::fromData(bytes).convertToFormat(QImage::Format_RGB888);
            require(!source.isNull() && source.width() <= 512 && source.height() <= 512,
                    "Unbounded photographic fixture.");
            const auto input = linear(source);
            const auto baseline = luma::renderCPU(input, {});
            painter.drawText(QRect(0, row * cellHeight, cellWidth, 22), Qt::AlignCenter, name);
            const auto originalPreview =
                image(baseline).scaled(192, 192, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.drawImage(QPoint((192 - originalPreview.width()) / 2,
                                     row * cellHeight + 24 + (192 - originalPreview.height()) / 2),
                              originalPreview);
            QJsonObject photo;
            int column = 1;
            for (const auto &entry : luma::builtinPresets()) {
                const auto rendered = luma::renderCPU(input, entry.preset.settings);
                require(luma::renderCPU(input, entry.preset.settings).rgba == rendered.rgba,
                        "Repeated look render is not deterministic.");
                const auto stats = metrics(rendered, baseline);
                if (entry.id == "builtin:natural")
                    require(rendered.rgba == baseline.rgba, "Natural changed neutral pixels.");
                else
                    require(stats[3].toDouble() > 0.2,
                            "An authored look has no observable effect.");
                require(stats[4].toDouble() <= metrics(baseline, baseline)[4].toDouble() + 2,
                        "Authored look introduces excessive additional clipping on QA photos.");
                photo.insert(entry.id, stats);
                painter.drawText(QRect(column * cellWidth, row * cellHeight, cellWidth, 22),
                                 Qt::AlignCenter, entry.preset.name);
                const auto preview =
                    image(rendered).scaled(192, 192, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawImage(QPoint(column * cellWidth + (192 - preview.width()) / 2,
                                         row * cellHeight + 24 + (192 - preview.height()) / 2),
                                  preview);
                ++column;
            }
            report.insert(name, photo);
            ++row;
        }
        painter.end();
        require(sheet.save(output + "/filters-contact-sheet.png"),
                "Cannot save photographic QA sheet.");
        QFile metricsFile(output + "/metrics.json");
        require(metricsFile.open(QIODevice::WriteOnly) &&
                    metricsFile.write(QJsonDocument(report).toJson()) > 0,
                "Cannot save photographic QA metrics.");
        metricsFile.close();
        const auto reference = QJsonDocument::fromJson(read(fixtures + "/reference.json")).object();
        require(report.keys() == reference.keys(), "Photographic reference set differs.");
        for (const auto &name : report.keys()) {
            const auto actual = report[name].toObject(), expected = reference[name].toObject();
            require(actual.keys() == expected.keys(), "Photographic look set differs.");
            for (const auto &id : actual.keys()) {
                const auto a = actual[id].toArray(), e = expected[id].toArray();
                require(a.size() == 53 && e.size() == 53, "Invalid look reference metrics.");
                for (qsizetype i = 0; i < a.size(); ++i)
                    require(std::abs(a[i].toDouble() - e[i].toDouble()) <= 0.4,
                            "Photographic look regression exceeds the reviewed tolerance.");
            }
        }
        std::cout << "Eight filters: licensed photographic regressions, identity, HDR, subsets and "
                     "JSON passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
