#pragma once
#include "core/jobs/Cancellation.h"
#include <QString>
#include <QUrl>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace luma {
struct WebDavGallery {
    QUrl parent;
    QString folder;
    QString user;
    QString password;
};

void publishWebDavGallery(const std::filesystem::path &catalog, const std::vector<std::string> &ids,
                          const std::string &title, const WebDavGallery &upload,
                          CancellationToken stop = {},
                          const std::function<void(double)> &progress = {});
} // namespace luma
