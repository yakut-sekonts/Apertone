#include "tests/ux/UxContext.h"
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQuickStyle>
#include <iostream>
namespace ux {
bool updaterScenario(Context &, const QString &, const QString &);
}
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
    parser.process(app);
    const auto root = parser.value("root");
    QDir().mkpath(root);
    if (parser.isSet("state-root"))
        QDir().mkpath(parser.value("state-root"));
    std::unique_ptr<ux::Context> context;
    try {
        context = std::make_unique<ux::Context>(root, parser.value("fixtures"), parser.value("qml"),
                                                parser.value("state-root"));
        if (parser.isSet("stage"))
            ux::restartScenario(*context, parser.value("case"), parser.value("stage"));
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
