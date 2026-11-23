#include "platform/macos/MacTetherCapture.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/io/Path.h"
#include "platform/macos/CFHandle.h"
#import <ImageCaptureCore/ImageCaptureCore.h>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>
#include <Security/SecTask.h>
#include <dispatch/dispatch.h>
#include <memory>

namespace {
constexpr qint64 minimumCaptureQuietPeriod = 1200;
constexpr qint64 maximumFileBytes = 1'300'000'000;

QString fromNative(NSString *value) {
    return value ? QString::fromNSString(value) : QString{};
}
QString identifier(ICDevice *device) {
    const auto uuid = fromNative(device.UUIDString);
    return uuid.isEmpty() ? fromNative(device.persistentIDString) : uuid;
}
bool canCapture(ICDevice *device) {
    return [device.capabilities containsObject:ICCameraDeviceCanTakePicture];
}
QString fileNameOnly(const QString &name) {
    if (name.isEmpty() || name == u"." || name == u".." || name.contains(u'\0') ||
        name.contains(u'/') || name.contains(u'\\'))
        return {};
    return name;
}
QString captureName(ICCameraFile *file) {
    NSString *original = file.originalFilename;
    return fileNameOnly(fromNative(original.length ? original : file.name));
}
} // namespace

@interface ApertoneMacCameraRequest : NSObject <ICDeviceBrowserDelegate, ICCameraDeviceDelegate> {
  @public
    QString id_, folder_;
    luma::CancellationToken stop_;
    luma::macos::TetherResult complete_;

  @private
    ICDeviceBrowser *browser_;
    ICCameraDevice *camera_;
    NSMutableArray<ICCameraFile *> *files_;
    NSProgress *download_;
    QTimer *timer_;
    QElapsedTimer phase_, lastFile_;
    std::shared_ptr<QTemporaryDir> staging_;
    QVariantList devices_;
    std::vector<std::filesystem::path> captured_;
    NSUInteger downloaded_;
    bool shutter_, downloadPending_, finished_;
}
- (void)start;
- (void)advance;
- (void)finish:(QString)error;
- (void)startDownload;
@end

@implementation ApertoneMacCameraRequest
- (void)start {
    if (stop_.isCancelled()) {
        [self finish:QString{}];
        return;
    }
    if (!id_.isEmpty()) {
        const QFileInfo destination(folder_);
        if (!destination.isDir() || !destination.isWritable()) {
            [self finish:QStringLiteral("Choose a writable capture folder.")];
            return;
        }
        staging_ = std::make_shared<QTemporaryDir>(
            QDir(folder_).filePath(QStringLiteral(".apertone-tether-XXXXXX")));
        if (!staging_->isValid()) {
            [self finish:QStringLiteral("Cannot create the camera capture staging folder.")];
            return;
        }
    }
    files_ = [NSMutableArray array];
    browser_ = [[ICDeviceBrowser alloc] init];
    browser_.delegate = self;
    timer_ = new QTimer(QCoreApplication::instance());
    timer_->setInterval(100);
    QObject::connect(timer_, &QTimer::timeout, timer_, [request = self] { [request advance]; });
    phase_.start();
    timer_->start();
    [browser_ start];
}

- (void)finish:(QString)error {
    if (finished_)
        return;
    finished_ = true;
    if (timer_) {
        timer_->stop();
        timer_->deleteLater();
        timer_ = nullptr;
    }
    [download_ cancel];
    if (downloadPending_)
        [camera_ cancelDownload];
    download_ = nil;
    camera_.delegate = nil;
    if (camera_.hasOpenSession)
        [camera_ requestCloseSessionWithOptions:nil
                                     completion:^(NSError *){
                                     }];
    browser_.delegate = nil;
    [browser_ stop];
    // The asynchronous download callback retains its own staging reference.
    camera_ = nil;
    browser_ = nil;
    auto complete = std::move(complete_);
    complete(std::move(devices_), std::move(captured_), std::move(error));
}

- (void)advance {
    if (finished_)
        return;
    if (stop_.isCancelled()) {
        [self finish:QString{}];
        return;
    }
    if (!camera_ && phase_.elapsed() >= 5000) {
        [self finish:id_.isEmpty() ? QString{}
                                   : QStringLiteral("The selected camera is no longer available.")];
        return;
    }
    if (camera_ && phase_.elapsed() >= (shutter_ ? 90000 : 30000)) {
        [self finish:shutter_ ? QStringLiteral("Timed out waiting for the camera photo.")
                              : QStringLiteral("The camera did not become ready.")];
        return;
    }
    if (shutter_ && downloaded_ != 0 && downloaded_ == files_.count && !downloadPending_ &&
        lastFile_.elapsed() >= minimumCaptureQuietPeriod)
        [self finish:QString{}];
}

- (void)deviceBrowser:(ICDeviceBrowser *)browser
         didAddDevice:(ICDevice *)device
           moreComing:(BOOL)moreComing {
    (void)browser;
    (void)moreComing;
    // ImageCaptureCore packs the transport location into ICDevice::type.
    if (finished_ || ![device isKindOfClass:[ICCameraDevice class]] ||
        (static_cast<NSUInteger>(device.type) &
         static_cast<NSUInteger>(ICDeviceLocationTypeMaskLocal)) == 0)
        return;
    const auto key = identifier(device);
    if (key.isEmpty())
        return;
    if (id_.isEmpty()) {
        if (canCapture(device)) {
            const auto name = fromNative(device.name);
            devices_.append(QVariantMap{{"id", key}, {"name", name.isEmpty() ? key : name}});
        }
        return;
    }
    if (key != id_ || camera_)
        return;
    camera_ = (ICCameraDevice *)device;
    camera_.delegate = self;
    phase_.restart();
    ICCameraDevice *selected = camera_;
    [selected requestOpenSessionWithOptions:nil
                                 completion:^(NSError *error) {
                                   dispatch_async(dispatch_get_main_queue(), ^{
                                     if (self->finished_) {
                                         if (!error && selected.hasOpenSession)
                                             [selected requestCloseSessionWithOptions:nil
                                                                           completion:^(NSError *){
                                                                           }];
                                     } else if (error) {
                                         [self finish:fromNative(error.localizedDescription)];
                                     }
                                   });
                                 }];
}

- (void)deviceBrowser:(ICDeviceBrowser *)browser
      didRemoveDevice:(ICDevice *)device
            moreGoing:(BOOL)moreGoing {
    (void)browser;
    (void)moreGoing;
    if (device == camera_)
        [self didRemoveDevice:device];
}

- (void)deviceBrowserDidEnumerateLocalDevices:(ICDeviceBrowser *)browser {
    (void)browser;
    if (id_.isEmpty())
        [self finish:QString{}];
}

- (void)deviceDidBecomeReadyWithCompleteContentCatalog:(ICCameraDevice *)camera {
    if (finished_ || stop_.isCancelled() || camera != camera_ || shutter_)
        return;
    if (!canCapture(camera)) {
        [self
            finish:QStringLiteral("The selected camera does not support remote shutter release.")];
        return;
    }
    shutter_ = true;
    phase_.restart();
    // macOS 14+ enables the standard remote shutter when the device advertises it.
    [camera requestTakePicture];
}

- (void)cameraDevice:(ICCameraDevice *)camera didAddItems:(NSArray<ICCameraItem *> *)items {
    if (finished_ || !shutter_ || stop_.isCancelled() || camera != camera_)
        return;
    for (ICCameraItem *item in items) {
        if (![item isKindOfClass:[ICCameraFile class]])
            continue;
        ICCameraFile *file = (ICCameraFile *)item;
        const auto name = captureName(file);
        if (!name.isEmpty() && file.fileSize > 0 && file.fileSize <= maximumFileBytes &&
            luma::PhotoDecoder::supportedExtension(luma::pathFromUtf8(name.toStdString())) &&
            ![files_ containsObject:file]) {
            if (files_.count >= 8) {
                [self finish:QStringLiteral("The camera returned too many files for one capture.")];
                return;
            }
            [files_ addObject:file];
            lastFile_.start();
        }
    }
    [self startDownload];
}

- (void)startDownload {
    if (finished_ || stop_.isCancelled() || downloadPending_ || downloaded_ >= files_.count)
        return;
    ICCameraFile *file = files_[downloaded_];
    const auto name = captureName(file);
    const QDir staging(staging_->path());
    const auto temporaryName = QString::number(downloaded_) + u'-' + name;
    const auto temporary = staging.filePath(temporaryName);
    NSURL *destination = [NSURL fileURLWithPath:staging.path().toNSString() isDirectory:YES];
    // Keep the staging directory alive until the native request relinquishes it.
    const auto holdStaging = staging_;
    downloadPending_ = true;
    download_ = [file
        requestDownloadWithOptions:@{
            ICDownloadsDirectoryURL : destination,
            ICSaveAsFilename : temporaryName.toNSString()
        }
                        completion:^(NSString *saved, NSError *error) {
                          dispatch_async(dispatch_get_main_queue(), ^{
                            if (self->finished_ || self->stop_.isCancelled())
                                return;
                            self->downloadPending_ = false;
                            self->download_ = nil;
                            const auto actualName = fileNameOnly(fromNative(saved));
                            const QFileInfo received(
                                actualName.isEmpty()
                                    ? temporary
                                    : QDir(holdStaging->path()).filePath(actualName));
                            if (error || !received.isFile() || received.isSymLink() ||
                                received.size() <= 0 || received.size() > maximumFileBytes) {
                                [self finish:error
                                                 ? fromNative(error.localizedDescription)
                                                 : QStringLiteral(
                                                       "Camera photo download did not complete.")];
                                return;
                            }
                            const QDir folder(self->folder_);
                            auto target = folder.filePath(name);
                            if (QFileInfo::exists(target))
                                target = folder.filePath(
                                    QUuid::createUuid().toString(QUuid::WithoutBraces) + u'-' +
                                    name);
                            if (!QFile::rename(received.filePath(), target)) {
                                [self finish:QStringLiteral("Cannot save the camera photo into the "
                                                            "capture folder.")];
                                return;
                            }
                            self->captured_.push_back(luma::pathFromUtf8(target.toStdString()));
                            ++self->downloaded_;
                            self->lastFile_.restart();
                            [self startDownload];
                          });
                        }];
}

- (void)didRemoveDevice:(ICDevice *)device {
    (void)device;
    [self finish:QStringLiteral("The camera was disconnected.")];
}
- (void)device:(ICDevice *)device didOpenSessionWithError:(NSError *)error {
    (void)device;
    if (error)
        [self finish:fromNative(error.localizedDescription)];
}
- (void)device:(ICDevice *)device didCloseSessionWithError:(NSError *)error {
    (void)device;
    [self finish:error ? fromNative(error.localizedDescription)
                       : QStringLiteral("The camera session closed before capture completed.")];
}
- (void)device:(ICDevice *)device didEncounterError:(NSError *)error {
    if (device == camera_ && error)
        [self finish:fromNative(error.localizedDescription)];
}
- (void)cameraDeviceDidEnableAccessRestriction:(ICDevice *)device {
    (void)device;
    [self finish:QStringLiteral("Unlock the camera to continue.")];
}
- (void)cameraDeviceDidRemoveAccessRestriction:(ICDevice *)device {
    (void)device;
}
- (void)cameraDeviceDidChangeCapability:(ICCameraDevice *)camera {
    (void)camera;
}
- (void)cameraDevice:(ICCameraDevice *)camera didRemoveItems:(NSArray<ICCameraItem *> *)items {
    (void)camera;
    (void)items;
}
- (void)cameraDevice:(ICCameraDevice *)camera didRenameItems:(NSArray<ICCameraItem *> *)items {
    (void)camera;
    (void)items;
}
- (void)cameraDevice:(ICCameraDevice *)camera didReceivePTPEvent:(NSData *)eventData {
    (void)camera;
    (void)eventData;
}
- (void)cameraDevice:(ICCameraDevice *)camera
    didReceiveThumbnail:(CGImageRef)thumbnail
                forItem:(ICCameraItem *)item
                  error:(NSError *)error {
    (void)camera;
    (void)thumbnail;
    (void)item;
    (void)error;
}
- (void)cameraDevice:(ICCameraDevice *)camera
    didReceiveMetadata:(NSDictionary *)metadata
               forItem:(ICCameraItem *)item
                 error:(NSError *)error {
    (void)camera;
    (void)metadata;
    (void)item;
    (void)error;
}
@end

bool luma::macos::cameraCaptureAvailable() {
    const CFHandle task(SecTaskCreateFromSelf(kCFAllocatorDefault));
    if (!task)
        return false;
    const CFHandle permission(SecTaskCopyValueForEntitlement(
        task.get(), CFSTR("com.apple.security.personal-information.photos-library"), nullptr));
    return permission.get() == kCFBooleanTrue;
}

void luma::macos::requestCameraCapture(const QString &id, const QString &folder,
                                       CancellationToken stop, TetherResult complete) {
    ApertoneMacCameraRequest *request = [[ApertoneMacCameraRequest alloc] init];
    request->id_ = id;
    request->folder_ = folder;
    request->stop_ = std::move(stop);
    request->complete_ = std::move(complete);
    [request start];
}
