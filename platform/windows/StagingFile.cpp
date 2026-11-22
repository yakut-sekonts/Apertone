#include "core/io/StagingFile.h"
#include "core/io/Path.h"
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <io.h>
#include <random>
#include <stdexcept>
#include <windows.h>

namespace luma {
namespace {
constexpr std::string_view alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

class Handle {
  public:
    explicit Handle(HANDLE value) : value(value) {}
    ~Handle() {
        if (valid())
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    bool valid() const {
        return value != INVALID_HANDLE_VALUE;
    }
    int descriptor() {
        const auto result = _open_osfhandle(reinterpret_cast<std::intptr_t>(value),
                                             _O_RDWR | _O_BINARY | _O_NOINHERIT);
        if (result >= 0)
            value = INVALID_HANDLE_VALUE;
        return result;
    }
    HANDLE value;
};

void validPrefix(std::string_view prefix) {
    if (prefix != ".luma-xmp-" && prefix != ".luma-backup-" && prefix != ".luma-derived-" &&
        prefix != ".luma-export-" && prefix != ".luma-diagnostics-" && prefix != ".luma-proxy-")
        throw std::invalid_argument("Unknown staging file kind.");
}

std::string marker(HANDLE file, const std::filesystem::path &path) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (file == INVALID_HANDLE_VALUE || GetFileType(file) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return {};
    return "LUMA-STAGING-WIN-V1\n" + pathUtf8(path) + '\n' +
           std::to_string(info.dwVolumeSerialNumber) + ':' + std::to_string(info.nFileIndexHigh) +
           ':' + std::to_string(info.nFileIndexLow);
}

std::string readMarker(HANDLE file) {
    std::array<char, 8193> bytes{};
    DWORD read = 0;
    if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
        read == 0 || read >= bytes.size())
        return {};
    return {bytes.data(), read};
}

bool removeHandle(HANDLE file) {
    FILE_DISPOSITION_INFO info{TRUE};
    return SetFileInformationByHandle(file, FileDispositionInfo, &info, sizeof info) != 0;
}

HANDLE create(const std::filesystem::path &p, DWORD access, DWORD sharing) {
    return CreateFileW(p.c_str(), access, sharing, nullptr, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}

HANDLE existing(const std::filesystem::path &p, DWORD access, DWORD sharing) {
    return CreateFileW(p.c_str(), access, sharing, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}
} // namespace

std::size_t StagingFile::cleanup(const std::filesystem::path &folder, std::string_view prefix) {
    validPrefix(prefix);
    std::error_code error;
    auto entry = std::filesystem::directory_iterator(folder, error);
    std::size_t removed = 0;
    for (; !error && entry != std::filesystem::directory_iterator{}; entry.increment(error)) {
        const auto name = pathUtf8(entry->path().filename());
        if (!name.starts_with(prefix) || !name.ends_with(".lock") ||
            name.size() != prefix.size() + 11 ||
            name.substr(prefix.size(), 6).find_first_not_of(alphabet) != std::string::npos)
            continue;
        const auto lockPath = std::filesystem::absolute(entry->path()).lexically_normal();
        const auto path = lockPath.parent_path() / name.substr(0, name.size() - 5);
        Handle lock(existing(lockPath, GENERIC_READ | DELETE, 0));
        if (!lock.valid() || marker(lock.value, lockPath).empty())
            continue;
        const auto recorded = readMarker(lock.value);
        if (!recorded.starts_with("LUMA-STAGING-WIN-V1\n" + pathUtf8(path) + '\n'))
            continue;
        Handle stage(existing(path, GENERIC_READ | DELETE, share));
        if (stage.valid()) {
            if (marker(stage.value, path) != recorded || !removeHandle(stage.value))
                continue;
            ++removed;
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            continue;
        }
        (void)removeHandle(lock.value);
    }
    return removed;
}

StagingFile::StagingFile(const std::filesystem::path &folder, std::string_view prefix) {
    validPrefix(prefix);
    (void)cleanup(folder, prefix);
    std::random_device random;
    std::uniform_int_distribution<std::size_t> index(0, alphabet.size() - 1);
    const auto directory = std::filesystem::absolute(folder).lexically_normal();
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::string basename(prefix);
        for (int i = 0; i < 6; ++i)
            basename += alphabet[index(random)];
        path_ = directory / basename;
        lockPath_ = directory / (basename + ".lock");
        Handle lock(create(lockPath_, GENERIC_READ | GENERIC_WRITE | DELETE, 0));
        if (!lock.valid()) {
            if (GetLastError() == ERROR_FILE_EXISTS)
                continue;
            throw std::runtime_error("Cannot create staging ownership record.");
        }
        Handle stage(create(path_, GENERIC_READ | GENERIC_WRITE, share));
        if (!stage.valid()) {
            const auto error = GetLastError();
            (void)removeHandle(lock.value);
            if (error == ERROR_FILE_EXISTS)
                continue;
            throw std::runtime_error("Cannot create staging file.");
        }
        const auto data = marker(stage.value, path_);
        DWORD written = 0;
        if (!data.empty() && data.size() <= 8192 &&
            WriteFile(lock.value, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
            written == data.size() && FlushFileBuffers(lock.value)) {
            descriptor_ = stage.descriptor();
            if (descriptor_ >= 0) {
                lockDescriptor_ = lock.descriptor();
                if (lockDescriptor_ >= 0)
                    return;
                _close(descriptor_);
                descriptor_ = -1;
            }
        }
        (void)DeleteFileW(path_.c_str());
        (void)removeHandle(lock.value);
        throw std::runtime_error("Cannot record staging file ownership.");
    }
    throw std::runtime_error("Cannot allocate a unique staging file.");
}

StagingFile::~StagingFile() {
    if (descriptor_ < 0)
        return;
    Handle stage(existing(path_, GENERIC_READ | DELETE, share));
    if (stage.valid()) {
        const auto recorded = marker(reinterpret_cast<HANDLE>(_get_osfhandle(descriptor_)), path_);
        if (!recorded.empty() && marker(stage.value, path_) == recorded)
            (void)removeHandle(stage.value);
    }
    _close(descriptor_);
    (void)removeHandle(reinterpret_cast<HANDLE>(_get_osfhandle(lockDescriptor_)));
    _close(lockDescriptor_);
}
} // namespace luma
