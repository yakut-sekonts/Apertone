#include "tests/ux/HistoryChaos.h"
#include "tests/ux/UxContext.h"
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QRegularExpression>
#include <iostream>
#include <optional>
namespace ux {
bool updaterScenario(Context &, const QString &, const QString &);
void prepareThumbnailCorruption(const QString &, const QString &);
void prepareMissingStartupSource(const QString &, const QString &);
} // namespace ux
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("Luma UX");
    app.setApplicationVersion(LUMA_VERSION);
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.addOption({"root", "Isolated public fixture evidence directory", "directory"});
    parser.addOption({"fixtures", "Generated fixture directory", "directory"});
    parser.addOption({"qml", "Production QML directory", "directory"});
    parser.addOption({"case", "Specification scenario ID", "id"});
    parser.addOption({"variant", "Isolated operation variant", "variant"});
    parser.addOption({"stage", "Multi-process test stage", "stage"});
    parser.addOption({"state-root", "Shared state for a process restart test", "directory"});
    parser.addOption({"seed", "Scoped UND-017 unsigned decimal seed", "integer"});
    parser.addOption({"actions", "Exact UND-017 semantic action count", "integer"});
    parser.addOption({"replay", "Immutable UND-017 replay schedule", "path"});
    parser.process(app);
    const auto root = parser.value("root");
    QDir().mkpath(root);
    if (parser.isSet("state-root"))
        QDir().mkpath(parser.value("state-root"));
    std::unique_ptr<ux::Context> context;
    try {
        const auto id = parser.value("case");
        std::optional<ux::HistoryChaosPlan> chaos;
        if (id == "UX-UND-017") {
            ux::require(parser.isSet("seed") && parser.isSet("actions") && !parser.isSet("stage") &&
                            !parser.isSet("state-root") && !parser.isSet("variant"),
                        "UND-017 requires explicit seed/actions and forbids stage/state/variant");
            const auto seedText = parser.value("seed");
            const auto countText = parser.value("actions");
            bool validSeed = false, validCount = false;
            const auto seed = seedText.toULongLong(&validSeed, 10);
            const auto count = countText.toInt(&validCount, 10);
            ux::require(
                validSeed && validCount &&
                    QRegularExpression(QRegularExpression::anchoredPattern("(0|[1-9][0-9]{0,19})"))
                        .match(seedText)
                        .hasMatch() &&
                    QRegularExpression(QRegularExpression::anchoredPattern("[1-9][0-9]{0,5}"))
                        .match(countText)
                        .hasMatch() &&
                    count >= 1 && count <= 100000,
                "UND-017 seed/count is invalid or exceeds the bounded evidence contract");
            // Actual Qt timer progress before any QML window/Context is created.
            // The deterministic generator yields to this timer, never fakes a beat.
            ux::Metrics preflightHeartbeat(root + "/heartbeat.json");
            chaos = ux::prepareHistoryChaosPlan(root, parser.value("fixtures"), seed, count,
                                                parser.value("replay"));
            std::cerr << "CHAOS preContext seed=" << seed << " actions=" << count
                      << " schedule_sha256=" << chaos->scheduleSHA256.toStdString() << '\n';
        } else {
            ux::require(
                !parser.isSet("seed") && !parser.isSet("actions") && !parser.isSet("replay"),
                "seed/actions/replay are supported only by UND-017; refusing ignored flags");
        }
        if (parser.value("case") == "UX-APP-004" && parser.value("stage") == "verify")
            ux::prepareMissingStartupSource(root, parser.value("state-root"));
        if (parser.value("case") == "UX-REC-009" && parser.value("stage") == "verify")
            ux::prepareThumbnailCorruption(root, parser.value("state-root"));
        const bool updaterProbe = id == "UX-UPD-005" || id == "UX-UPD-006";
        context = std::make_unique<ux::Context>(root, parser.value("fixtures"), parser.value("qml"),
                                                parser.value("state-root"), updaterProbe);
        if (chaos)
            ux::runHistoryChaos(*context, *chaos);
        else if (parser.isSet("stage"))
            ux::restartScenario(*context, parser.value("case"), parser.value("stage"));
        else if (parser.value("case") == "UX-BAT-013")
            ux::require(ux::updaterScenario(*context, "UX-UPD-003", "batch"),
                        "Batch restart adapter unavailable");
        else if (!ux::updaterScenario(*context, parser.value("case"), parser.value("variant")))
            ux::basicScenario(*context, parser.value("case"));
        context->saved();
        context->checkData();
        ux::require(!context->metrics.json()["hard_hang"].toBool(), "Main-loop hard hang");
        context->finish(parser.value("case"), "PASS");
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        if (context)
            context->finish(parser.value("case"), "FAIL", QString::fromUtf8(failure.what()));
        else
            ux::writeJson(root + "/result.json", {{"id", parser.value("case")},
                                                  {"status", "FAIL"},
                                                  {"message", QString::fromUtf8(failure.what())}});
        return 1;
    }
}
