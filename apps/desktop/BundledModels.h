#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace luma::desktop {
enum class BundledModel { denoise, sky };
inline QString bundledModelPath(BundledModel model) {
#ifdef _WIN32
    if (model != BundledModel::sky)
        return {};
    const QFileInfo file(QCoreApplication::applicationDirPath() + QStringLiteral("/Models/skyseg.onnx"));
    return file.isFile() && !file.isSymLink() ? file.absoluteFilePath() : QString();
#else
    const auto filename = model == BundledModel::denoise ? QStringLiteral("LumaDenoise.mlmodelc")
                                                         : QStringLiteral("LumaSky.mlmodelc");
    const auto path = QDir::cleanPath(QCoreApplication::applicationDirPath() +
                                      QStringLiteral("/../Resources/Models/") + filename);
    const QFileInfo file(path);
    return file.isDir() && !file.isSymLink() ? path : QString();
#endif
}
} // namespace luma::desktop
