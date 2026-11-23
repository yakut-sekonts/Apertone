#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace fs = std::filesystem;

constexpr wchar_t executable[] = L"Apertone.exe";
constexpr wchar_t helper[] = L"ApertoneUpdate.exe";
constexpr wchar_t manifest[] = L"UPDATE-FILES.txt";
constexpr wchar_t provenance[] = L"BUILD-ORIGIN.txt";
constexpr wchar_t stagePrefix[] = L".apertone-update-";

class Handle {
  public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() {
        if (valid())
            CloseHandle(value_);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    bool valid() const {
        return value_ && value_ != INVALID_HANDLE_VALUE;
    }
    HANDLE get() const {
        return value_;
    }

  private:
    HANDLE value_;
};

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

int comparePaths(std::wstring_view a, std::wstring_view b) {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                                static_cast<int>(b.size()), TRUE);
}

bool samePath(const fs::path &a, const fs::path &b) {
    return comparePaths(a.native(), b.native()) == CSTR_EQUAL;
}

struct PathLess {
    bool operator()(const std::wstring &a, const std::wstring &b) const {
        return comparePaths(a, b) == CSTR_LESS_THAN;
    }
};

bool asciiAlphaNumeric(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
}

DWORD ordinaryAttributes(const fs::path &path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    require(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
            "An update path is missing or is a Windows reparse point.");
    return attributes;
}

void requireOrdinaryPath(const fs::path &path, bool directory) {
    require(path.is_absolute() && samePath(path.lexically_normal(), path),
            "The update path must be absolute and normalized.");
    require(static_cast<bool>(ordinaryAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) == directory,
            "An update path has the wrong file type.");
    for (auto parent = path.parent_path(); !parent.empty() && parent != path.root_path();
         parent = parent.parent_path())
        require(ordinaryAttributes(parent) & FILE_ATTRIBUTE_DIRECTORY,
                "An update parent is not a directory.");
}

bool absent(const fs::path &path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return false;
    const auto error = GetLastError();
    require(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND,
            "An update path could not be inspected.");
    return true;
}

std::wstring fromUtf8(std::string_view text) {
    if (text.empty())
        return {};
    const auto length = static_cast<int>(text.size());
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    require(count > 0, "The package manifest is not valid UTF-8.");
    std::wstring result(count, L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, result.data(),
                                count) == count,
            "The package manifest could not be decoded.");
    return result;
}

std::string readSmallFile(const fs::path &path) {
    requireOrdinaryPath(path, false);
    constexpr auto maximumSize = 1024 * 1024;
    const auto size = fs::file_size(path);
    require(size > 0 && size <= maximumSize, "The package metadata has an invalid size.");
    std::ifstream stream(path, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    require(
        static_cast<bool>(stream.read(text.data(), static_cast<std::streamsize>(text.size()))) &&
            stream.peek() == std::char_traits<char>::eof(),
        "The package metadata could not be read.");
    return text;
}

bool validRelativePath(std::wstring_view path) {
    if (path.empty() || path.ends_with(L'/'))
        return false;
    while (!path.empty()) {
        const auto separator = path.find(L'/');
        const auto part = path.substr(0, separator);
        if (part.empty() || part.back() == L'.' || part.back() == L' ' ||
            part.find_first_of(L"\\:<>\"|?*") != std::wstring_view::npos)
            return false;
        for (const wchar_t c : part)
            if (c < 32)
                return false;
        if (separator == std::wstring_view::npos)
            break;
        path.remove_prefix(separator + 1);
    }
    return true;
}

void validateTree(const fs::path &root) {
    requireOrdinaryPath(root, true);
    auto listing = readSmallFile(root / manifest);
    if (listing.starts_with("\xEF\xBB\xBF"))
        listing.erase(0, 3);
    std::istringstream lines(std::move(listing));
    std::set<std::wstring, PathLess> files;
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto relative = fromUtf8(line);
        require(validRelativePath(relative) && comparePaths(relative, manifest) != CSTR_EQUAL &&
                    files.insert(std::move(relative)).second,
                "The package manifest contains an unsafe or duplicate entry.");
    }
    require(files.contains(executable) && files.contains(provenance),
            "Required Apertone program files are missing from the package manifest.");
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        if (ordinaryAttributes(entry.path()) & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        const auto relative = entry.path().lexically_relative(root).generic_wstring();
        if (comparePaths(relative, manifest) != CSTR_EQUAL)
            require(files.erase(relative) == 1,
                    "The Apertone folder contains files outside its package manifest.");
    }
    require(files.empty(), "The Apertone folder is missing files from its package manifest.");
    const auto origin = readSmallFile(root / provenance);
    require(origin.find("Repository: https://github.com/yakut-sekonts/Apertone") !=
                    std::string::npos &&
                origin.find("Platform: Windows x64") != std::string::npos,
            "The Apertone package provenance is invalid.");
}

struct Paths {
    fs::path install;
    fs::path stage;
    fs::path package;
    fs::path previous;
};

Paths updatePaths(const fs::path &install) {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
    require(length && length < buffer.size(), "The update helper path is unavailable.");
    const fs::path module(std::wstring_view(buffer.data(), length));
    requireOrdinaryPath(module, false);
    const auto stage = module.parent_path();
    const auto name = stage.filename().wstring();
    constexpr std::wstring_view prefix(stagePrefix);
    require(install.has_filename() && samePath(module.filename(), helper) &&
                name.starts_with(prefix) && name.size() == prefix.size() + 6 &&
                samePath(stage.parent_path(), install.parent_path()) && !samePath(stage, install),
            "The update helper must run from an Apertone staging folder beside the installation.");
    for (const auto character : std::wstring_view(name).substr(prefix.size()))
        require(asciiAlphaNumeric(character), "The update staging folder name is invalid.");
    return {install, stage, stage / L"package", stage / L"previous"};
}

void validate(const Paths &paths) {
    require(absent(paths.previous), "A previous update needs recovery in the staging folder.");
    validateTree(paths.install);
    validateTree(paths.package);
}

DWORD processId(std::wstring_view text) {
    require(!text.empty(), "The Apertone process identifier is missing.");
    std::uint64_t result = 0;
    for (const wchar_t character : text) {
        require(character >= L'0' && character <= L'9',
                "The Apertone process identifier is invalid.");
        result = result * 10 + static_cast<unsigned>(character - L'0');
        require(result <= std::numeric_limits<DWORD>::max(),
                "The Apertone process identifier is invalid.");
    }
    return static_cast<DWORD>(result);
}

void waitForApertone(DWORD pid, const fs::path &application) {
    if (!pid)
        return;
    const Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.valid()) {
        require(GetLastError() == ERROR_INVALID_PARAMETER,
                "The Apertone process could not be opened for the update.");
        return; // The application can finish before the helper is scheduled.
    }
    std::array<wchar_t, 32768> image{};
    DWORD size = image.size();
    if (!QueryFullProcessImageNameW(process.get(), 0, image.data(), &size)) {
        require(WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0,
                "The running Apertone executable could not be identified.");
        return;
    }
    require(samePath(fs::canonical(fs::path(std::wstring_view(image.data(), size))), application),
            "The supplied process does not belong to this Apertone installation.");
    require(WaitForSingleObject(process.get(), 60000) == WAIT_OBJECT_0,
            "Apertone did not close in time. The update was not applied.");
}

std::wstring quoteArgument(std::wstring_view argument) {
    std::wstring quoted(1, L'"');
    std::size_t slashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        quoted.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        quoted += character;
    }
    quoted.append(slashes * 2, L'\\');
    return quoted + L'"';
}

bool launch(const fs::path &install, std::wstring_view catalog) {
    const auto application = install / executable;
    DWORD binaryType = 0;
    if (!GetBinaryTypeW(application.c_str(), &binaryType) || binaryType != SCS_64BIT_BINARY)
        return false;
    auto command = quoteArgument(application.native());
    if (!catalog.empty())
        command += L" --catalog " + quoteArgument(catalog);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        install.c_str(), &startup, &child))
        return false;
    const Handle process(child.hProcess), thread(child.hThread);
    if (WaitForSingleObject(process.get(), 1000) != WAIT_OBJECT_0)
        return true;
    DWORD code = 0;
    return GetExitCodeProcess(process.get(), &code) && code == 0;
}

bool restore(const Paths &paths, bool swapped) {
    std::error_code error;
    if (swapped) {
        fs::rename(paths.install, paths.package, error);
        if (error)
            return false;
    }
    fs::rename(paths.previous, paths.install, error);
    return !error;
}

void applyUpdate(const Paths &paths, DWORD pid, std::wstring_view catalog) {
    const auto lockPath = paths.install.parent_path() / L".apertone-update.lock";
    const Handle lock(CreateFileW(lockPath.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
    require(lock.valid(), "Another update is running or the installation folder is not writable.");
    const auto original = readSmallFile(paths.install / provenance);
    waitForApertone(pid, paths.install / executable);

    bool parked = false, swapped = false;
    try {
        validate(paths);
        require(readSmallFile(paths.install / provenance) == original,
                "The Apertone installation changed while waiting to update.");
        fs::rename(paths.install, paths.previous);
        parked = true;
        fs::rename(paths.package, paths.install);
        swapped = true;
        require(launch(paths.install, catalog), "The new Apertone version could not start.");
    } catch (...) {
        if (parked)
            require(restore(paths, swapped), "Apertone could not be restored. The previous version "
                                             "is in the update staging folder.");
        if (pid) {
            try {
                validateTree(paths.install);
                if (readSmallFile(paths.install / provenance) == original)
                    launch(paths.install, catalog);
            } catch (...) {
                // Preserve the original update error and leave the installation intact.
            }
        }
        throw;
    }

    // Cleanup is optional after a successful relaunch. Recheck the parked tree
    // before touching it, and preserve it if any unexpected files appeared.
    try {
        require(samePath(paths.previous.parent_path(), paths.stage), "Invalid backup path.");
        validateTree(paths.previous);
        std::error_code ignored;
        fs::remove_all(paths.previous, ignored);
    } catch (...) {
    }
}
} // namespace

int wmain(int argc, wchar_t **argv) {
    // Startup loader errors must return to the helper so it can restore the
    // old application. A system error dialog could stall that recovery.
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    bool silent = true;
    try {
        require(argc == 3 || argc == 4, "Invalid Apertone update helper arguments.");
        const bool preflight = std::wstring_view(argv[1]) == L"--validate";
        if (preflight) {
            require(argc == 3, "Invalid Apertone update preflight arguments.");
            validate(updatePaths(fs::path(argv[2])));
            return 0;
        }
        const auto pid = processId(argv[2]);
        silent = pid == 0;
        const auto paths = updatePaths(fs::path(argv[1]));
        applyUpdate(paths, pid, argc == 4 ? std::wstring_view(argv[3]) : std::wstring_view{});
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "%s\n", failure.what());
        if (!silent) {
            std::wstring message = L"The Apertone update could not be applied.";
            try {
                message += L"\n\n" + fromUtf8(failure.what());
            } catch (...) {
            }
            MessageBoxW(nullptr, message.c_str(), L"Apertone update", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
}
