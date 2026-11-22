#include "core/io/FileSystem.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <csignal>
#endif

namespace luma::files {
#ifdef _WIN32
namespace {
int fail(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        errno = EEXIST;
        break;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        errno = EACCES;
        break;
    case ERROR_NOT_SAME_DEVICE:
        errno = EXDEV;
        break;
    default:
        errno = EIO;
        break;
    }
    return -1;
}
} // namespace
#endif

int openRead(const std::filesystem::path &path, bool rejectLinks) {
#ifdef _WIN32
    if (!rejectLinks)
        return _wopen(path.c_str(), _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return fail(GetLastError());
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
        CloseHandle(handle);
        return fail(ERROR_ACCESS_DENIED);
    }
    const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle),
                                           _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    if (descriptor < 0)
        CloseHandle(handle);
    return descriptor;
#else
    return ::open(path.c_str(), O_RDONLY | (rejectLinks ? O_NOFOLLOW : 0));
#endif
}

int createExclusive(const std::filesystem::path &path) {
#ifdef _WIN32
    return _wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT,
                  _S_IREAD | _S_IWRITE);
#else
    return ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
#endif
}

int flush(int descriptor) {
#ifdef _WIN32
    return _commit(descriptor);
#else
    return fsync(descriptor);
#endif
}

int syncFile(const std::filesystem::path &path) {
#ifdef _WIN32
    const int descriptor = _wopen(path.c_str(), _O_RDWR | _O_BINARY | _O_NOINHERIT);
#else
    const int descriptor = openRead(path);
#endif
    if (descriptor < 0)
        return -1;
    const int result = flush(descriptor);
    const int error = errno;
    close(descriptor);
    errno = error;
    return result;
}

int syncDirectory(const std::filesystem::path &path) {
#ifdef _WIN32
    // Windows has no directory equivalent of fsync. Publication and quarantine
    // use same-volume MoveFileExW with MOVEFILE_WRITE_THROUGH.
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)
               ? 0
               : fail(ERROR_PATH_NOT_FOUND);
#else
    return syncFile(path);
#endif
}

int unlinkFile(const std::filesystem::path &path) {
#ifdef _WIN32
    return _wunlink(path.c_str());
#else
    return ::unlink(path.c_str());
#endif
}

int move(const std::filesystem::path &source, const std::filesystem::path &target, bool replace) {
#ifdef _WIN32
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(source.c_str(), target.c_str(), flags))
        return fail(GetLastError());
    return 0;
#else
    return replace ? ::rename(source.c_str(), target.c_str())
                   : renamex_np(source.c_str(), target.c_str(), RENAME_EXCL);
#endif
}

int publish(const std::filesystem::path &source, const std::filesystem::path &target) {
#ifdef _WIN32
    return move(source, target);
#else
    return ::link(source.c_str(), target.c_str());
#endif
}

std::int64_t processID() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

bool processExited(std::int64_t owner) {
    if (owner <= 0 || owner == processID() || owner > std::numeric_limits<int>::max())
        return false;
#ifdef _WIN32
    const auto process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(owner));
    if (!process)
        return GetLastError() == ERROR_INVALID_PARAMETER;
    const bool exited = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
    CloseHandle(process);
    return exited;
#else
    return kill(static_cast<pid_t>(owner), 0) != 0 && errno == ESRCH;
#endif
}
} // namespace luma::files
