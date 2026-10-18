#include "tests/ux/HistoryChaos.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <functional>
#include <iostream>
#include <stdexcept>

// Pure oracle/evidence tests, not native UX qualification.
namespace {
void check(bool value, const char *reason) {
    if (!value)
        throw std::runtime_error(reason);
}
void rejects(const std::function<void()> &operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid replay/model request was accepted");
}
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              file.write(bytes) == bytes.size(),
          "Cannot write temporary oracle-only fixture");
}
void capAndBranch() {
    ux::HistoryChaosModel model;
    for (int index = 0; index < 100; ++index)
        model.apply("edit", "contrast", 1);
    for (int index = 0; index < 100; ++index)
        model.apply("edit", "shadows", 1);
    for (int index = 0; index < 5; ++index)
        model.apply("edit", "vibrance", 1);
    luma::EditDocument final;
    final.contrast = 100;
    final.shadows = 100;
    final.vibrance = 5;
    check(model.current() == final && model.undoDepth() == 200 && model.redoDepth() == 0,
          "205 known edits did not produce the documented 200-entry capacity");
    for (int index = 0; index < 205; ++index)
        model.apply("undo");
    luma::EditDocument firstRetained;
    firstRetained.contrast = 5;
    check(model.current() == firstRetained && model.undoDepth() == 0 && model.redoDepth() == 200,
          "Independent retained boundary was not the fifth original edit");
    for (int index = 0; index < 205; ++index)
        model.apply("redo");
    check(model.current() == final && model.undoDepth() == 200 && model.redoDepth() == 0,
          "Independent Redo did not reconstruct the known final document");
    model.apply("undo");
    auto before = final;
    before.vibrance = 4;
    check(model.current() == before && model.redoDepth() == 1, "Missing known Redo branch");
    model.apply("edit", "contrast", 1); // at +100: a no-op must preserve the branch
    check(model.current() == before && model.redoDepth() == 1, "Clamped no-op discarded Redo");
    model.apply("edit", "contrast", -1);
    before.contrast = 99;
    check(model.current() == before && model.redoDepth() == 0, "New edit retained stale Redo");
    model.apply("redo");
    check(model.current() == before, "Disabled Redo changed the new branch");
    rejects([&] { model.apply("edit", "exposure", 1); });
    rejects([&] { model.apply("edit", "contrast", 2); });
    rejects([&] { model.apply("undo", "contrast", 1); });
}
void fullDocumentHash() {
    const luma::EditDocument initial;
    const auto before = ux::historyDocumentHash(initial);
    auto changed = initial;
    const std::vector<double luma::EditDocument::*> numbers{
        &luma::EditDocument::exposure,
        &luma::EditDocument::temperature,
        &luma::EditDocument::tint,
        &luma::EditDocument::contrast,
        &luma::EditDocument::highlights,
        &luma::EditDocument::shadows,
        &luma::EditDocument::whites,
        &luma::EditDocument::blacks,
        &luma::EditDocument::vibrance,
        &luma::EditDocument::saturation,
        &luma::EditDocument::sharpenAmount,
        &luma::EditDocument::sharpenRadius,
        &luma::EditDocument::sharpenDetail,
        &luma::EditDocument::sharpenMasking,
        &luma::EditDocument::noiseLuminance,
        &luma::EditDocument::noiseLuminanceDetail,
        &luma::EditDocument::noiseLuminanceContrast,
        &luma::EditDocument::noiseColor,
        &luma::EditDocument::noiseColorDetail,
        &luma::EditDocument::noiseColorSmoothness,
        &luma::EditDocument::curveShadows,
        &luma::EditDocument::curveDarks,
        &luma::EditDocument::curveLights,
        &luma::EditDocument::curveHighlights};
    for (auto member : numbers) {
        changed = initial;
        changed.*member += 1;
        check(ux::historyDocumentHash(changed) != before,
              "A full-document numeric field was omitted");
    }
    const std::vector<std::function<void(luma::EditDocument &)>> mutations{
        [](auto &d) { d.customWhiteBalance = true; },
        [](auto &d) { d.enabledCorrections ^= 2; },
        [](auto &d) { d.crop.flipHorizontal = true; },
        [](auto &d) { d.crop.width = .75; },
        [](auto &d) { d.lens.profile = "literal-profile"; },
        [](auto &d) { d.lens.chromaticAberration = false; },
        [](auto &d) { d.transform.guide[7] = .1; },
        [](auto &d) { d.transform.guided = true; },
        [](auto &d) { d.curves[2].points.push_back({.5, .6}); },
        [](auto &d) { d.hsl[7].luminance = 1; },
        [](auto &d) { d.localMasks.push_back({}); },
        [](auto &d) {
            luma::LocalMask m;
            m.id = "mask";
            m.strokes.push_back({});
            m.strokes[0].points.push_back({.1, .2});
            d.localMasks.push_back(m);
        },
        [](auto &d) {
            luma::LocalMask m;
            luma::MaskComponent c;
            c.id = "component";
            c.geometry.colorTolerance = .3;
            m.components.push_back(c);
            d.localMasks.push_back(m);
        }};
    for (const auto &mutate : mutations) {
        changed = initial;
        mutate(changed);
        check(ux::historyDocumentHash(changed) != before,
              "A nested typed document group was omitted");
    }
    changed = initial;
    changed.contrast = -0.0;
    check(ux::historyDocumentHash(changed) == before,
          "Equivalent signed zero was not canonicalized");
}
void plansAndReplay() {
    QTemporaryDir temporary;
    check(temporary.isValid(), "Cannot create oracle test directory");
    const auto fixtures = temporary.path() + "/fixture-hashes-only";
    check(QDir().mkpath(fixtures), "Cannot create hash-only fixture directory");
    // These are bytes for pure plan tests, not decodable or claimed native images.
    for (int index = 0; index < 10; ++index)
        write(fixtures + QString("/Photo-%1.png").arg(index, 4, 10, QChar('0')),
              "oracle-only-fixture-" + QByteArray::number(index));
    const auto one = ux::historyChaosPlan(123456, 10000, fixtures);
    const auto same = ux::historyChaosPlan(123456, 10000, fixtures);
    const auto other = ux::historyChaosPlan(123457, 10000, fixtures);
    check(one.bytes == same.bytes && one.rows.size() == 10000 && one.rows != other.rows,
          "Seeded schedule is not deterministic or lost exact action count");
    const auto first = temporary.path() + "/first";
    const auto saved = ux::prepareHistoryChaosPlan(first, fixtures, 123456, 10000, {});
    const auto replayed = ux::prepareHistoryChaosPlan(temporary.path() + "/replay", fixtures,
                                                      123456, 10000, first + "/replay.jsonl");
    check(saved.bytes == replayed.bytes && saved.scheduleSHA256 == replayed.scheduleSHA256,
          "Saved schedule was regenerated differently on replay");
    const auto denied = temporary.path() + "/denied";
    rejects([&] {
        ux::prepareHistoryChaosPlan(denied, fixtures, 123457, 10000, first + "/replay.jsonl");
    });
    rejects([&] {
        ux::prepareHistoryChaosPlan(denied, fixtures, 123456, 9999, first + "/replay.jsonl");
    });
    check(!QFileInfo::exists(denied + "/seed.txt"), "Rejected replay created acceptance evidence");
    auto corrupt = saved.bytes;
    corrupt[corrupt.size() / 2] = '!';
    write(temporary.path() + "/corrupt.jsonl", corrupt);
    rejects([&] {
        ux::prepareHistoryChaosPlan(denied, fixtures, 123456, 10000,
                                    temporary.path() + "/corrupt.jsonl");
    });
    rejects([&] { ux::historyChaosPlan(1, 0, fixtures); });
    rejects([&] { ux::historyChaosPlan(1, 100001, fixtures); });
    write(fixtures + "/Photo-0000.png", "changed-fixture");
    rejects([&] {
        ux::prepareHistoryChaosPlan(denied, fixtures, 123456, 10000, first + "/replay.jsonl");
    });
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("chaos-oracle-test-v1");
    try {
        capAndBranch();
        fullDocumentHash();
        plansAndReplay();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
