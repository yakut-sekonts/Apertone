#include "platform/macos/MacTetherCapture.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using namespace luma;
    CancellationSource cancelled;
    cancelled.cancel();
    bool cancellationPassed = false, invalidFolderPassed = false, discoveryPassed = false;
    macos::requestCameraCapture(
        {}, {}, cancelled.token(), [&](auto cameras, auto paths, auto error) {
            cancellationPassed = cameras.empty() && paths.empty() && error.isEmpty();
        });
    const QTemporaryDir scratch;
    if (!scratch.isValid())
        return 1;
    macos::requestCameraCapture(
        QStringLiteral("camera-fixture"), scratch.filePath(QStringLiteral("missing")), {},
        [&](auto cameras, auto paths, auto error) {
            invalidFolderPassed = cameras.empty() && paths.empty() && !error.isEmpty();
        });

    QEventLoop loop;
    QTimer::singleShot(8000, &loop, &QEventLoop::quit);
    QTimer::singleShot(0, &loop, [&] {
        macos::requestCameraCapture({}, {}, {}, [&](auto cameras, auto paths, auto error) {
            discoveryPassed = paths.empty() && error.isEmpty();
            for (const auto &camera : cameras) {
                const auto fields = camera.toMap();
                discoveryPassed = discoveryPassed && !fields.value("id").toString().isEmpty() &&
                                  !fields.value("name").toString().isEmpty();
            }
            loop.quit();
        });
    });
    loop.exec();
    if (!cancellationPassed || !invalidFolderPassed || !discoveryPassed) {
        std::cerr << "macOS camera cancellation, input validation or discovery failed\n";
        return 1;
    }
    return 0;
}
