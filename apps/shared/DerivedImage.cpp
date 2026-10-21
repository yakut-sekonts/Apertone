#include "apps/shared/DerivedImage.h"
#include "core/catalog/Identity.h"
#include "core/catalog/Catalog.h"
#include "platform/macos/FileHasher.h"
#include "core/export/DngWriter.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace luma {
std::filesystem::path createDerivedImage(const std::filesystem::path &catalog,
                                         const LinearOutput &image, std::string_view provenance,
                                         CancellationToken stop,
                                         const std::function<void(double)> &progress) {
    image.validate();
    checkCancellation(stop);
    if (provenance.size() > 1048576)
        throw std::runtime_error("Derived image metadata exceeds its budget.");
    const auto directory = std::filesystem::absolute(catalog).parent_path() / (catalog.filename().string() + ".luma-derived");
    std::filesystem::create_directories(directory);
    const auto target = directory / (newID() + ".dng");
    std::string pattern = (directory / ".luma-derived-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    const int descriptor = mkstemp(name.data());
    if (descriptor < 0)
        throw std::runtime_error("Cannot create derived image temporary file.");
    close(descriptor);
    const std::filesystem::path temporary(name.data());
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } cleanup{temporary};
    const auto profile = linearSRGBProfile();
    const auto bytes = std::span(reinterpret_cast<const std::uint8_t *>(provenance.data()), provenance.size());
    writeLinearDNG(image, rgbToXYZD50(profile),
        [](std::span<const float> source, std::span<float> destination) {
            std::copy(source.begin(), source.end(), destination.begin());
        }, temporary, "Luma Linear sRGB", bytes, stop, progress, DngSamples::floating32);
    const int input = ::open(temporary.c_str(), O_RDONLY);
    if (input < 0) throw std::runtime_error("Cannot reopen derived image.");
    const int synced = fsync(input);
    close(input);
    if (synced != 0) throw std::runtime_error("Cannot sync derived image.");
    const auto checksum = macos::sha256File(temporary, stop);
    Catalog database(catalog);
    const DerivedImageRecord record{target, checksum, image.width, image.height, getpid()};
    checkCancellation(stop);
    database.stageDerived(record);
    if (link(temporary.c_str(), target.c_str()) != 0) {
        database.discardDerived(target);
        throw std::runtime_error("Cannot install derived image: " + std::string(std::strerror(errno)));
    }
    // Installation is the commit point. Cancellation after it keeps the output.
    const int parent = ::open(directory.c_str(), O_RDONLY);
    if (parent >= 0) { (void)fsync(parent); close(parent); }
    // Persist registration even when the UI has closed or cancelled meanwhile.
    // If registration fails, the journal and installed file remain recoverable.
    database.finishDerived(record);
    return target;
}
} // namespace luma
