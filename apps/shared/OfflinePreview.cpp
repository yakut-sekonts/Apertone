#include "apps/shared/OfflinePreview.h"
#include "core/image/Bayer.h"
#include "core/io/FileSystem.h"
#include "core/io/Path.h"
#include "core/io/StagingFile.h"
#include "core/render/CpuRenderer.h"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <fstream>
#include <span>

namespace luma {
namespace {
constexpr std::array<char, 12> signature{'A', 'P', 'E', 'R', 'T', 'O', 'N', 'E', 'P', 'R', 'X', 1};
constexpr int maxEdge = 1600;
constexpr std::uintmax_t diskBudget = 1ULL << 30;
static_assert(std::endian::native == std::endian::little && sizeof(float) == 4 &&
              sizeof(double) == 8);

bool validDigest(std::string_view value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == value.npos;
}

std::filesystem::path directory(const std::filesystem::path &catalog) {
    return pathWithSuffix(std::filesystem::absolute(catalog), ".luma-cache") /
           "offline-previews-v1";
}

template <class Sample> void write(std::ostream &out, std::span<const Sample> data) {
    const auto bytes = std::as_bytes(data);
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw std::runtime_error("Cannot write offline preview.");
}

template <class Sample> void read(std::istream &in, std::span<Sample> data) {
    const auto bytes = std::as_writable_bytes(data);
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in)
        throw std::runtime_error("Offline preview is truncated.");
}

template <class Transfer, class Calibration>
void matrices(Calibration &calibration, const Transfer &transfer) {
    transfer(calibration.cameraToSRGB);
    transfer(calibration.asShotGains);
    transfer(calibration.analogBalance);
    transfer(calibration.asShotNeutral);
    for (auto &profile : calibration.dngProfiles) {
        transfer(profile.colorMatrix);
        transfer(profile.cameraCalibration);
        transfer(profile.forwardMatrix);
    }
}

void writeCalibration(std::ostream &out, const RawColorCalibration &value) {
    matrices(value, [&](const auto &numbers) { write(out, std::span<const float>(numbers)); });
    write(out, std::span(&value.dngProfileCount, 1));
    for (const auto &profile : value.dngProfiles) {
        write(out, std::span(&profile.calibrationTemperature, 1));
        const std::uint8_t forward = profile.hasForwardMatrix;
        write(out, std::span(&forward, 1));
    }
}

RawColorCalibration readCalibration(std::istream &in) {
    RawColorCalibration value;
    matrices(value, [&](auto &numbers) {
        read(in, std::span<float>(numbers));
        if (!std::ranges::all_of(numbers, [](float x) { return std::isfinite(x); }))
            throw std::runtime_error("Invalid offline preview color calibration.");
    });
    read(in, std::span(&value.dngProfileCount, 1));
    if (value.dngProfileCount > value.dngProfiles.size())
        throw std::runtime_error("Invalid offline preview profile count.");
    for (auto &profile : value.dngProfiles) {
        read(in, std::span(&profile.calibrationTemperature, 1));
        std::uint8_t forward = 0;
        read(in, std::span(&forward, 1));
        if (!std::isfinite(profile.calibrationTemperature) || forward > 1)
            throw std::runtime_error("Invalid offline preview color profile.");
        profile.hasForwardMatrix = forward != 0;
    }
    return value;
}

void trimPreviews(const std::filesystem::path &folder, const std::filesystem::path &newest) {
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type time;
        std::uintmax_t bytes;
    };
    std::vector<Entry> entries;
    std::uintmax_t total = 0;
    std::error_code error;
    for (auto item = std::filesystem::directory_iterator(folder, error);
         !error && item != std::filesystem::directory_iterator{}; item.increment(error)) {
        const auto &path = item->path();
        if (path.extension() != ".aprx" || !validDigest(pathUtf8(path.stem())))
            continue;
        std::error_code status;
        if (!std::filesystem::is_regular_file(item->symlink_status(status)) || status)
            continue;
        const auto size = item->file_size(status);
        if (status)
            continue;
        const auto time = item->last_write_time(status);
        if (!status) {
            total += size;
            if (path != newest)
                entries.push_back({path, time, size});
        }
    }
    if (total <= diskBudget)
        return;
    std::ranges::sort(entries, {}, &Entry::time);
    for (const auto &entry : entries) {
        if (std::filesystem::remove(entry.path, error))
            total -= entry.bytes;
        if (total <= diskBudget)
            break;
    }
}
} // namespace

std::optional<OfflinePreview> loadOfflinePreview(const std::filesystem::path &catalog,
                                                 const std::string &checksum,
                                                 CancellationToken stop) {
    if (!validDigest(checksum))
        return std::nullopt;
    const auto path = directory(catalog) / (checksum + ".aprx");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;
    checkCancellation(stop);
    std::array<char, signature.size()> magic{};
    std::array<char, 64> digest{};
    std::array<std::uint32_t, 3> dimensions{};
    read(input, std::span<char>(magic));
    read(input, std::span<char>(digest));
    read(input, std::span<std::uint32_t>(dimensions));
    if (magic != signature || std::string_view(digest.data(), digest.size()) != checksum ||
        dimensions[0] > maxEdge || dimensions[1] > maxEdge || dimensions[2] > 1)
        throw std::runtime_error("Invalid offline preview. Reconnect the original.");
    auto calibration = readCalibration(input);
    FloatImage image(static_cast<int>(dimensions[0]), static_cast<int>(dimensions[1]));
    read(input, std::span<float>(image.rgb));
    if (dimensions[2]) {
        image.alpha.resize(pixelCount(image.width, image.height));
        read(input, std::span<float>(image.alpha));
    }
    if (input.peek() != std::char_traits<char>::eof() ||
        !std::ranges::all_of(image.rgb, [](float x) { return std::isfinite(x); }))
        throw std::runtime_error("Invalid offline preview pixels.");
    image.validateCoverage();
    checkCancellation(stop);
    // Access time is managed explicitly since Windows can disable atime updates.
    std::error_code ignored;
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ignored);
    return OfflinePreview{std::move(image), std::move(calibration)};
}

void saveOfflinePreview(const std::filesystem::path &catalog, const std::string &checksum,
                        const FloatImage &preview, const RawColorCalibration &calibration,
                        CancellationToken stop, bool repair) {
    if (!validDigest(checksum))
        throw std::runtime_error("Invalid offline preview fingerprint.");
    const auto folder = directory(catalog);
    const auto target = folder / (checksum + ".aprx");
    if (!repair && std::filesystem::exists(target))
        return;
    checkCancellation(stop);
    const auto image = standardPreview(preview, maxEdge, stop);
    const std::array<std::uint32_t, 3> dimensions{static_cast<std::uint32_t>(image.width),
                                                  static_cast<std::uint32_t>(image.height),
                                                  image.alpha.empty() ? 0U : 1U};
    std::filesystem::create_directories(folder);
    StagingFile staging(folder, ".luma-proxy-");
    std::ofstream output(staging.path(), std::ios::binary | std::ios::trunc);
    write(output, std::span<const char>(signature));
    write(output, std::span<const char>(checksum));
    write(output, std::span<const std::uint32_t>(dimensions));
    writeCalibration(output, calibration);
    write(output, std::span<const float>(image.rgb));
    if (!image.alpha.empty())
        write(output, std::span<const float>(image.alpha));
    output.close();
    checkCancellation(stop);
    if (!output || files::syncFile(staging.path()) != 0)
        throw std::runtime_error("Cannot sync offline preview.");
    const auto installed =
        repair ? files::move(staging.path(), target, true) : files::publish(staging.path(), target);
    if (installed != 0 && (repair || errno != EEXIST))
        throw std::runtime_error("Cannot install offline preview.");
    (void)files::syncDirectory(folder);
    trimPreviews(folder, target);
}

bool hasOfflinePreview(const std::filesystem::path &catalog, const std::string &checksum) {
    std::error_code error;
    return validDigest(checksum) &&
           std::filesystem::is_regular_file(directory(catalog) / (checksum + ".aprx"), error);
}
} // namespace luma
