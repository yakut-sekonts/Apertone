#include "OwnedVolume.h"
#include "tests/ux/UxDriver.h"
#import <Foundation/Foundation.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ux {
namespace {
constexpr qint64 imageSize = 64LL * 1024 * 1024;
constexpr qint64 maxCommandBytes = 1024LL * 1024;
constexpr qint64 maxPinnedFileBytes = 8LL * 1024 * 1024;
constexpr auto sentinelName = ".luma-ux-volume-owner.json";

QString number(quint64 value) {
    return QString::number(value);
}
QString errorText() {
    return QString::fromLocal8Bit(std::strerror(errno));
}
QByteArray json(const QJsonObject &value) {
    return QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
}
bool missing(const QString &path) {
    struct stat value{};
    const auto encoded = QFile::encodeName(path);
    if (::lstat(encoded.constData(), &value) == 0)
        return false;
    require(errno == ENOENT, "Cannot establish absence of owned path: " + path);
    return true;
}
void syncDirectory(const QString &path) {
    const auto encoded = QFile::encodeName(path);
    const int descriptor = ::open(encoded.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    const bool synced = descriptor >= 0 && ::fsync(descriptor) == 0;
    if (descriptor >= 0)
        ::close(descriptor);
    require(synced, "Cannot sync owned evidence directory: " + path);
}
void exclusiveFile(const QString &path, const QByteArray &data) {
    const auto encoded = QFile::encodeName(path);
    const int fd = ::open(encoded.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        throw std::runtime_error(
            ("Cannot create exclusively owned volume evidence: " + path).toStdString());
    QFile file;
    if (!file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        throw std::runtime_error(("Cannot own new evidence descriptor: " + path).toStdString());
    }
    require(file.write(data) == data.size() && file.flush() && ::fsync(fd) == 0,
            "Cannot durably write owned volume evidence: " + path);
    file.close();
    syncDirectory(QFileInfo(path).absolutePath());
}
NSDictionary *plist(const QByteArray &data) {
    require(!data.isEmpty() && data.size() <= maxCommandBytes,
            "Owned volume command returned absent/oversized plist");
    NSError *error = nil;
    id value = [NSPropertyListSerialization
        propertyListWithData:[NSData dataWithBytes:data.constData() length:data.size()]
                     options:NSPropertyListImmutable
                      format:nil
                       error:&error];
    require(value && [value isKindOfClass:[NSDictionary class]],
            "Owned volume command did not return a dictionary plist: " +
                (error ? QString::fromNSString(error.localizedDescription) : QString()));
    return value;
}
QString string(NSDictionary *value, NSString *key) {
    id field = value[key];
    require(field && [field isKindOfClass:[NSString class]],
            "Owned plist lacks string field: " + QString::fromNSString(key));
    NSString *text = field;
    return QString::fromNSString(text);
}
bool boolean(NSDictionary *value, NSString *key) {
    id field = value[key];
    require(field && CFGetTypeID((__bridge CFTypeRef)field) == CFBooleanGetTypeID(),
            "Owned plist lacks boolean field: " + QString::fromNSString(key));
    return [field boolValue];
}
qint64 integer(NSDictionary *value, NSString *key) {
    id field = value[key];
    require(field && [field isKindOfClass:[NSNumber class]] &&
                CFGetTypeID((__bridge CFTypeRef)field) != CFBooleanGetTypeID() &&
                !CFNumberIsFloatType((__bridge CFNumberRef)field),
            "Owned plist lacks numeric field: " + QString::fromNSString(key));
    return [field longLongValue];
}

// Never destroy a still-running QProcess: its destructor can block the GUI.
// On timeout kill only our own subprocess and let its own terminal signal delete
// the wrapper. No callback refers to Driver/OwnedVolume/stack storage afterwards.
struct ProcessDelete {
    void operator()(QProcess *process) const noexcept {
        if (process->state() == QProcess::NotRunning) {
            delete process;
            return;
        }
        QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::errorOccurred, process,
                         [process](QProcess::ProcessError error) {
                             if (error == QProcess::FailedToStart)
                                 process->deleteLater();
                         });
        process->kill();
    }
};
} // namespace

struct OwnedVolume::Impl {
    enum class State : std::uint8_t { empty, detached, attached, uncertain };
    Driver &driver;
    QString root, image, mount, nonce, device, uuid;
    QFile journal;
    QElapsedTimer clock;
    QMap<QString, QString> originals;
    QByteArray sentinel;
    State state = State::empty;
    quint64 imageDevice = 0, imageInode = 0, mountDevice = 0;
    fsid_t filesystemID{};
    int generation = 0, ordinal = 0, commandOrdinal = 0;
    bool readOnly = false;
    bool finalized = false;

    Impl(Driver &input, const QString &caseRoot) : driver(input) {
        require(QCoreApplication::instance() &&
                    QThread::currentThread() == QCoreApplication::instance()->thread(),
                "Owned volume fixture requires the live application's GUI thread");
        const QFileInfo parent(caseRoot);
        require(parent.isDir() && !parent.isSymLink() && !parent.canonicalFilePath().isEmpty(),
                "Owned volume requires an existing private case directory");
        QTemporaryDir fresh(parent.canonicalFilePath() + "/owned-volume-XXXXXX");
        require(fresh.isValid(), "Cannot create fresh private volume evidence directory");
        fresh.setAutoRemove(false); // Preserve image/evidence even on partial attach failure.
        root = QFileInfo(fresh.path()).canonicalFilePath();
        const auto encoded = QFile::encodeName(root);
        require(!root.isEmpty() && root.startsWith(parent.canonicalFilePath() + '/') &&
                    ::chmod(encoded.constData(), 0700) == 0,
                "Owned volume directory escaped its private parent");
        image = root + "/owned.dmg";
        nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mount = "/Volumes/LumaUX-" + nonce;
        journal.setFileName(root + "/events.jsonl");
        require(journal.open(QIODevice::WriteOnly | QIODevice::NewOnly),
                "Cannot exclusively create owned volume event journal");
        syncDirectory(root);
        syncDirectory(parent.canonicalFilePath());
        clock.start();
        event("helper_created", {{"physical_usb_exercised", false},
                                 {"helper_alone_qualifies_ux", false},
                                 {"filesystem", "hfs"},
                                 {"layout", "NONE"},
                                 {"blank_type", "UDIF (UDRW)"}});
    }
    QString stateName() const {
        switch (state) {
        case State::empty:
            return "empty";
        case State::detached:
            return "detached";
        case State::attached:
            return "attached";
        case State::uncertain:
            return "uncertain";
        }
        return "invalid";
    }
    QJsonObject observed() const {
        QJsonObject result{{"state", stateName()}, {"image", image},
                           {"mount", mount},       {"device", device},
                           {"volume_uuid", uuid},  {"generation", generation},
                           {"read_only", readOnly}};
        struct stat status{};
        struct statfs filesystem{};
        const auto encoded = QFile::encodeName(mount);
        const int statusResult = ::lstat(encoded.constData(), &status);
        result["mount_path_exists"] = statusResult == 0;
        if (::statfs(encoded.constData(), &filesystem) == 0) {
            result["actual_mount_point"] = QString::fromLocal8Bit(filesystem.f_mntonname);
            result["actual_mount_from"] = QString::fromLocal8Bit(filesystem.f_mntfromname);
            result["actual_filesystem"] = QString::fromLocal8Bit(filesystem.f_fstypename);
            result["actual_read_only"] = (filesystem.f_flags & MNT_RDONLY) != 0;
            result["actual_fsid"] = QJsonArray{filesystem.f_fsid.val[0], filesystem.f_fsid.val[1]};
        } else
            result["statfs_error"] = errorText();
        if (statusResult == 0)
            result["actual_mount_device"] = number(static_cast<quint64>(status.st_dev));
        return result;
    }
    void event(const QString &action, const QJsonObject &extra = {}) {
        QJsonObject entry{{"ordinal", ++ordinal}, {"monotonic_ms", clock.elapsed()},
                          {"action", action},     {"nonce", nonce},
                          {"actual", observed()}, {"extra", extra}};
        const auto data = json(entry);
        require(journal.write(data) == data.size() && journal.flush() &&
                    ::fsync(static_cast<int>(journal.handle())) == 0,
                "Cannot durably retain owned volume boundary evidence");
    }
    QByteArray command(const QString &program, const QStringList &arguments, const QString &label,
                       int timeout = 60000) {
        const auto prefix =
            QString("command-%1-%2").arg(++commandOrdinal, 3, 10, QChar('0')).arg(label);
        QJsonArray argv;
        for (const auto &argument : arguments)
            argv.append(argument);
        event("command_intent",
              {{"program", program}, {"argv", argv}, {"prefix", prefix}, {"timeout_ms", timeout}});
        qint64 childPID = 0;
        std::unique_ptr<QProcess, ProcessDelete> process(new QProcess);
        // Receiver lifetime ends before ProcessDelete; timed-out process cleanup
        // cannot later call into this stack or an expired OwnedVolume.
        QObject observation;
        QObject::connect(process.get(), &QProcess::started, &observation,
                         [&] { childPID = process->processId(); });
        process->setProcessChannelMode(QProcess::SeparateChannels);
        process->setWorkingDirectory(root);
        process->setStandardInputFile(QProcess::nullDevice()); // Never an interactive prompt.
        process->start(program, arguments);
        QByteArray output, error;
        auto collect = [&] {
            output += process->readAllStandardOutput();
            error += process->readAllStandardError();
            require(output.size() <= maxCommandBytes && error.size() <= maxCommandBytes,
                    "Owned volume command exceeded its bounded diagnostic output");
        };
        try {
            driver.wait(
                [&] {
                    collect();
                    return process->state() == QProcess::NotRunning;
                },
                "owned volume command " + label, timeout);
            collect();
        } catch (...) {
            // Outcome may be unknown (DiskImages may already have attached a device).
            // Do not guess a device or run another storage command in cleanup.
            state = State::uncertain;
            exclusiveFile(root + '/' + prefix + ".stdout", output);
            exclusiveFile(root + '/' + prefix + ".stderr", error);
            event("command_failed_before_process_cleanup",
                  {{"prefix", prefix},
                   {"child_pid", childPID},
                   {"process_state", static_cast<int>(process->state())},
                   {"error", process->errorString()}});
            throw;
        }
        exclusiveFile(root + '/' + prefix + ".stdout", output);
        exclusiveFile(root + '/' + prefix + ".stderr", error);
        const bool success = childPID > 0 && process->exitStatus() == QProcess::NormalExit &&
                             process->exitCode() == 0 &&
                             process->error() != QProcess::FailedToStart;
        event("command_finished", {{"prefix", prefix},
                                   {"child_pid", childPID},
                                   {"exit_code", process->exitCode()},
                                   {"exit_status", static_cast<int>(process->exitStatus())},
                                   {"error", process->errorString()},
                                   {"success", success}});
        require(success, "Owned volume command failed: " + label + ": " + process->errorString() +
                             ": " + QString::fromUtf8(error));
        return output;
    }
    void backingIdentity() const {
        struct stat status{};
        const auto encoded = QFile::encodeName(image);
        require(::lstat(encoded.constData(), &status) == 0 && S_ISREG(status.st_mode) &&
                    status.st_uid == ::getuid() && status.st_nlink == 1 &&
                    status.st_size == imageSize && QFileInfo(image).canonicalFilePath() == image &&
                    static_cast<quint64>(status.st_dev) == imageDevice &&
                    static_cast<quint64>(status.st_ino) == imageInode,
                "Owned backing file identity changed; storage commands refused");
    }
    QJsonObject liveIdentity(bool requireSentinel = true) {
        backingIdentity();
        require(QRegularExpression("^/dev/disk[0-9]+$").match(device).hasMatch(),
                "Owned attach did not yield one exact whole HFS+ device");
        NSDictionary *info =
            plist(command("/usr/sbin/diskutil", {"info", "-plist", device}, "owned-info", 15000));
        const auto observedUUID = QUuid(string(info, @"VolumeUUID"));
        require(
            !observedUUID.isNull() && string(info, @"DeviceNode") == device &&
                string(info, @"DeviceIdentifier") == device.mid(5) &&
                string(info, @"ParentWholeDisk") == device.mid(5) && boolean(info, @"WholeDisk") &&
                !boolean(info, @"Internal") && string(info, @"BusProtocol") == "Disk Image" &&
                string(info, @"VirtualOrPhysical") == "Virtual" &&
                string(info, @"FilesystemType") == "hfs" && string(info, @"MountPoint") == mount &&
                string(info, @"VolumeName") == "LumaUX-" + nonce &&
                integer(info, @"TotalSize") == imageSize &&
                boolean(info, @"WritableVolume") == !readOnly,
            "Exact returned device is not the private HFS+ image/mount identity");
        const auto actualUUID = observedUUID.toString(QUuid::WithoutBraces);
        require(uuid.isEmpty() || actualUUID == uuid,
                "Volume UUID changed; refusing to act on a replacement device");
        struct stat mountStatus{}, hostStatus{}, deviceStatus{};
        struct statfs filesystem{};
        const auto mountBytes = QFile::encodeName(mount);
        const auto hostBytes = QFile::encodeName(root);
        const auto deviceBytes = QFile::encodeName(device);
        require(::lstat(mountBytes.constData(), &mountStatus) == 0 &&
                    S_ISDIR(mountStatus.st_mode) &&
                    ::stat(hostBytes.constData(), &hostStatus) == 0 &&
                    mountStatus.st_dev != hostStatus.st_dev &&
                    ::lstat(deviceBytes.constData(), &deviceStatus) == 0 &&
                    S_ISBLK(deviceStatus.st_mode) &&
                    ::statfs(mountBytes.constData(), &filesystem) == 0 &&
                    QString::fromLocal8Bit(filesystem.f_mntonname) == mount &&
                    QString::fromLocal8Bit(filesystem.f_mntfromname) == device &&
                    QString::fromLocal8Bit(filesystem.f_fstypename) == "hfs" &&
                    ((filesystem.f_flags & MNT_RDONLY) != 0) == readOnly,
                "Actual statfs device/type/mount/read-only identity does not match attach");
        if (state == State::attached)
            require(static_cast<quint64>(mountStatus.st_dev) == mountDevice &&
                        filesystem.f_fsid.val[0] == filesystemID.val[0] &&
                        filesystem.f_fsid.val[1] == filesystemID.val[1],
                    "Attachment generation was replaced before its storage action");
        if (requireSentinel) {
            const auto path = mount + '/' + sentinelName;
            struct stat status{};
            require(::lstat(QFile::encodeName(path).constData(), &status) == 0 &&
                        S_ISREG(status.st_mode) && status.st_dev == mountStatus.st_dev &&
                        status.st_size == sentinel.size(),
                    "Owned volume sentinel is absent, aliased or on another filesystem");
            const int descriptor =
                ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW);
            require(descriptor >= 0, "Cannot open own sentinel without following links");
            QFile file;
            if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
                ::close(descriptor);
                require(false, "Cannot own sentinel descriptor");
            }
            require(file.read(sentinel.size() + 1) == sentinel,
                    "Owned volume nonce/backing-file sentinel changed");
        }
        return {{"volume_uuid", actualUUID},
                {"mount_device", number(static_cast<quint64>(mountStatus.st_dev))},
                {"fsid", QJsonArray{filesystem.f_fsid.val[0], filesystem.f_fsid.val[1]}}};
    }
    void attachImage(bool readonly, bool first) {
        backingIdentity();
        require(missing(mount), "Exact owned mountpoint is occupied; remount refused");
        readOnly = readonly;
        state = State::uncertain; // Attach can partially succeed before any validation fails.
        const auto data = command("/usr/bin/hdiutil",
                                  {"attach", image, "-plist", "-nobrowse", "-noautoopen",
                                   readonly ? "-readonly" : "-readwrite", "-mountpoint", mount},
                                  first ? "attach-first" : "remount");
        NSDictionary *result = plist(data);
        id entities = result[@"system-entities"];
        require(entities && [entities isKindOfClass:[NSArray class]] && [entities count] == 1,
                "Owned attach has unsupported multi-device topology");
        NSArray *list = entities;
        id entity = list[0];
        require([entity isKindOfClass:[NSDictionary class]], "Owned attach entity is invalid");
        NSDictionary *entry = entity;
        device = string(entry, @"dev-entry");
        require(string(entry, @"mount-point") == mount && string(entry, @"volume-kind") == "hfs",
                "Owned attach plist mounted an unexpected filesystem/path");
        const auto identity = liveIdentity(!first);
        if (first) {
            uuid = identity["volume_uuid"].toString();
            sentinel = json({{"nonce", nonce},
                             {"image", image},
                             {"volume_uuid", uuid},
                             {"backing_device", number(imageDevice)},
                             {"backing_inode", number(imageInode)}});
            exclusiveFile(mount + '/' + sentinelName, sentinel);
        }
        struct stat status{};
        require(::stat(QFile::encodeName(mount).constData(), &status) == 0,
                "Cannot snapshot owned mount device");
        struct statfs filesystem{};
        require(::statfs(QFile::encodeName(mount).constData(), &filesystem) == 0,
                "Cannot snapshot owned filesystem generation");
        mountDevice = static_cast<quint64>(status.st_dev);
        filesystemID = filesystem.f_fsid;
        ++generation;
        state = State::attached;
        liveIdentity();
        verifyPins();
        event("attachment_accepted", {{"identity", identity}});
    }
    void verifyPins() {
        for (auto it = originals.cbegin(); it != originals.cend(); ++it) {
            const auto &relative = it.key();
            const auto path = mount + '/' + relative;
            require(!relative.isEmpty() && !QDir::isAbsolutePath(relative) &&
                        relative == QDir::cleanPath(relative) &&
                        !relative.split('/').contains("..") && !relative.contains(QChar(0)) &&
                        QFileInfo(path).canonicalFilePath() == path,
                    "Pinned source path escaped the real private mount");
            const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW);
            if (fd < 0)
                throw std::runtime_error("Cannot read pinned own source");
            QFile file;
            if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
                ::close(fd);
                throw std::runtime_error("Cannot own pinned source descriptor");
            }
            struct stat status{};
            require(::fstat(fd, &status) == 0 && S_ISREG(status.st_mode) &&
                        static_cast<quint64>(status.st_dev) == mountDevice && status.st_size >= 0 &&
                        status.st_size <= maxPinnedFileBytes,
                    "Pinned source is not a bounded regular file on this owned filesystem");
            QCryptographicHash hash(QCryptographicHash::Sha256);
            require(hash.addData(&file) && QString::fromLatin1(hash.result().toHex()) == it.value(),
                    "Pinned owned-volume source SHA256 differs from independent host original");
        }
    }
    void detachImage(bool auditOriginals = true) {
        require(state == State::attached, "Owned volume has no confirmed attachment to detach");
        liveIdentity();
        // Data corruption is an acceptance failure, not loss of ownership. A
        // failure-path destructor still detaches its verified own filesystem.
        if (auditOriginals)
            verifyPins(); // All file descriptors close before detach invocation.
        const auto identity = liveIdentity(); // Recheck after any potentially slow source reads.
        event("detach_before_action", {{"identity", identity}});
        const auto oldDevice = device;
        state = State::uncertain;
        command("/usr/bin/hdiutil", {"detach", oldDevice}, "detach");
        // Invalidate the token before further event dispatch: /dev/diskN may be reused.
        device.clear();
        driver.wait(
            [&] {
                struct statfs filesystem{};
                const bool stillOurMount =
                    ::statfs(QFile::encodeName(mount).constData(), &filesystem) == 0 &&
                    QString::fromLocal8Bit(filesystem.f_mntonname) == mount &&
                    QString::fromLocal8Bit(filesystem.f_mntfromname) == oldDevice;
                return !stillOurMount && missing(oldDevice);
            },
            "exact owned device detached and pinned filesystem absent", 10000);
        state = State::detached;
        event("detach_confirmed", {{"invalidated_device", oldDevice}});
    }
    void cleanup() noexcept {
        try {
            event("cleanup_before_any_action");
            if (state == State::attached)
                detachImage(false); // Ownership only; a failed source oracle cannot block cleanup.
            else if (state == State::uncertain)
                event("cleanup_refused_uncertain_attachment");
            event("cleanup_finished_image_and_evidence_preserved",
                  {{"checked_finalize_completed", finalized}});
        } catch (const std::exception &error) {
            try {
                event("cleanup_failed_no_force_or_fallback", {{"error", error.what()}});
            } catch (...) {
                std::fputs("OwnedVolume cleanup evidence failed; image preserved.\n", stderr);
            }
        } catch (...) {
            std::fputs("OwnedVolume cleanup failed; no force or arbitrary-device fallback.\n",
                       stderr);
        }
    }
};

OwnedVolume::OwnedVolume(Driver &driver, const QString &root)
    : impl_(std::make_unique<Impl>(driver, root)) {}
OwnedVolume::~OwnedVolume() noexcept {
    impl_->cleanup();
}
void OwnedVolume::createAndAttach() {
    require(impl_->state == Impl::State::empty && missing(impl_->image),
            "Owned image creation requires one fresh unused helper");
    impl_->command("/usr/bin/hdiutil",
                   {"create", "-size", "64m", "-fs", "HFS+", "-layout", "NONE", "-type", "UDIF",
                    "-volname", "LumaUX-" + impl_->nonce, impl_->image},
                   "create");
    struct stat status{};
    require(::lstat(QFile::encodeName(impl_->image).constData(), &status) == 0 &&
                S_ISREG(status.st_mode),
            "Own blank image was not created as a regular file");
    impl_->imageDevice = static_cast<quint64>(status.st_dev);
    impl_->imageInode = static_cast<quint64>(status.st_ino);
    impl_->backingIdentity();
    impl_->state = Impl::State::detached;
    impl_->attachImage(false, true);
}
void OwnedVolume::detach() {
    impl_->detachImage();
}
void OwnedVolume::remount(bool readonly) {
    require(!impl_->finalized && impl_->state == Impl::State::detached && !impl_->uuid.isEmpty(),
            "Remount requires a previously confirmed detached own volume");
    impl_->attachImage(readonly, false);
}
void OwnedVolume::finalize() {
    require(!impl_->finalized, "Owned volume was already finalized");
    if (impl_->state == Impl::State::attached)
        impl_->detachImage();
    require(impl_->state == Impl::State::detached && !impl_->uuid.isEmpty() &&
                missing(impl_->mount),
            "Successful case requires a proven final detach and absent own mountpoint");
    impl_->backingIdentity();
    impl_->event("checked_finalize_completed_before_case_success");
    impl_->finalized = true;
}
void OwnedVolume::pinOriginals(const QMap<QString, QString> &expected) {
    require(impl_->state == Impl::State::attached && impl_->originals.isEmpty() &&
                !expected.isEmpty() && expected.size() <= 16,
            "Original pins require an attached volume and one explicit nonempty fixture set");
    for (auto it = expected.cbegin(); it != expected.cend(); ++it)
        require(QRegularExpression("^[0-9a-f]{64}$").match(it.value()).hasMatch(),
                "Original pin is not an independently supplied SHA256 hex value");
    impl_->originals = expected;
    impl_->liveIdentity();
    impl_->verifyPins();
    QJsonObject pinned;
    for (auto it = expected.cbegin(); it != expected.cend(); ++it)
        pinned[it.key()] = it.value();
    impl_->event("independent_original_pins_verified", {{"pins", pinned}});
}
void OwnedVolume::verifyOriginals() {
    require(impl_->state == Impl::State::attached, "Cannot verify original pins while detached");
    impl_->liveIdentity();
    impl_->verifyPins();
}
void OwnedVolume::checkpoint(const QString &label, const QJsonObject &extra) {
    impl_->event(label, extra);
}
QString OwnedVolume::evidenceRoot() const {
    return impl_->root;
}
QString OwnedVolume::imagePath() const {
    return impl_->image;
}
QString OwnedVolume::mountPath() const {
    return impl_->mount;
}
QString OwnedVolume::volumeUUID() const {
    return impl_->uuid;
}
bool OwnedVolume::attached() const {
    return impl_->state == Impl::State::attached;
}
bool OwnedVolume::detached() const {
    return impl_->state == Impl::State::detached;
}
} // namespace ux
