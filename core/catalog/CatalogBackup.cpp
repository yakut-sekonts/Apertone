#include "core/catalog/Catalog.h"
#include "core/image/Bayer.h"
#include <sqlite3.h>
#include <chrono>
#include <fcntl.h>
#include <thread>
#include <unistd.h>

namespace luma {
void Catalog::backupTo(const std::filesystem::path &destination, CancellationToken stop,
                        const std::function<void(double)> &progress) const {
    checkCancellation(stop);
    const auto target = std::filesystem::absolute(destination);
    if (std::filesystem::exists(target))
        throw std::runtime_error("Backup destination already exists.");
    std::filesystem::create_directories(target.parent_path());
    std::string pattern = (target.parent_path() / ".luma-backup-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
    const int descriptor = mkstemp(name.data());
    if (descriptor < 0) throw std::runtime_error("Cannot create catalog backup.");
    close(descriptor);
    struct Temporary {
        std::filesystem::path path;
        ~Temporary() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            std::filesystem::remove(path.string() + "-journal", ignored);
            std::filesystem::remove(path.string() + "-wal", ignored);
            std::filesystem::remove(path.string() + "-shm", ignored);
        }
    } temporary{name.data()};
    sqlite3 *output = nullptr;
    if (sqlite3_open_v2(temporary.path.c_str(), &output, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (output) sqlite3_close(output);
        throw std::runtime_error("Cannot open catalog backup.");
    }
    struct Connection {
        sqlite3 *db;
        ~Connection() { if (db) sqlite3_close(db); }
    } connection{output};
    sqlite3_backup *copy = sqlite3_backup_init(output, "main", db_, "main");
    if (!copy) throw std::runtime_error("Cannot start catalog snapshot.");
    struct Copy {
        sqlite3_backup *value;
        ~Copy() { if (value) sqlite3_backup_finish(value); }
    } backup{copy};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    for (;;) {
        checkCancellation(stop);
        const int status = sqlite3_backup_step(copy, 256);
        if (status == SQLITE_DONE) break;
        if (status != SQLITE_OK && status != SQLITE_BUSY && status != SQLITE_LOCKED)
            throw std::runtime_error("Catalog snapshot failed.");
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Catalog remained busy during backup.");
        if (status == SQLITE_OK) {
            deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            const int total = sqlite3_backup_pagecount(copy);
            if (progress && total > 0)
                progress(1.0 - static_cast<double>(sqlite3_backup_remaining(copy)) / total);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    const int finished = sqlite3_backup_finish(copy);
    backup.value = nullptr;
    if (finished != SQLITE_OK) throw std::runtime_error("Cannot finish catalog snapshot.");
    if (sqlite3_close(output) != SQLITE_OK)
        throw std::runtime_error("Cannot finalize catalog backup.");
    connection.db = nullptr;
    const int file = ::open(temporary.path.c_str(), O_RDONLY);
    if (file < 0) throw std::runtime_error("Cannot reopen catalog backup.");
    const int synced = fsync(file); close(file);
    if (synced != 0) throw std::runtime_error("Cannot sync catalog backup.");
    checkCancellation(stop);
    if (link(temporary.path.c_str(), target.c_str()) != 0)
        throw std::runtime_error("Cannot install catalog backup without replacing a file.");
    const int folder = ::open(target.parent_path().c_str(), O_RDONLY);
    if (folder >= 0) { (void)fsync(folder); close(folder); }
    if (progress) progress(1);
}
} // namespace luma
