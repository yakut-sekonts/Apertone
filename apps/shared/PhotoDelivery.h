#pragma once
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class QPrinter;

namespace luma {
enum class DeliveryFormat { contactSheet, photoBook, webGallery, slideshow };
int photoDeliveryPageCount(DeliveryFormat format, std::size_t photoCount);
void printPhotos(QPrinter &printer, const std::filesystem::path &catalog,
                 const std::vector<std::string> &ids, DeliveryFormat format,
                 const std::string &title, const std::filesystem::path &proofProfile = {},
                 CancellationToken stop = {}, const std::function<void(double)> &progress = {});
void deliverPhotos(const std::filesystem::path &catalog, const std::vector<std::string> &ids,
                   const std::filesystem::path &destination, DeliveryFormat format,
                   const std::string &title, const std::filesystem::path &proofProfile = {},
                   CancellationToken stop = {}, const std::function<void(double)> &progress = {});
} // namespace luma
