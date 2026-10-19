#pragma once
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <memory>

namespace ux {
class Driver;

// Test-only, single HFS+ filesystem on a new private 64 MiB UDIF/UDRW image.
// Construct as a local inside the runner child/case, destroy before Context.
// The parent's real screen flock must cover child startup through wait/teardown;
// this helper neither releases nor substitutes for that lock. Driver and the live
// QGuiApplication must outlive this object. No scenarios,
// production state changes, physical USB claims or automatic retries are supplied.
class OwnedVolume final {
  public:
    OwnedVolume(Driver &driver, const QString &existingPrivateCaseRoot);
    ~OwnedVolume() noexcept;
    OwnedVolume(const OwnedVolume &) = delete;
    OwnedVolume &operator=(const OwnedVolume &) = delete;

    void createAndAttach();
    void detach();
    void remount(bool readOnly = false);
    // Mandatory checked end of a successful case, before returning PASS.
    // Performs final ordinary detach if needed and confirms no owned mount path.
    // Destructor cleanup is failure recovery only and cannot replace this call.
    void finalize();

    // Paths are relative regular files already created by the calling fixture.
    // Expected SHA256 comes from independent host originals, never from this
    // helper's readback. Up to 16 small files (8 MiB each); larger workloads need
    // a separate asynchronous hashing design. Pins are verified before detach
    // and after every remount. No open source FD survives into detach.
    void pinOriginals(const QMap<QString, QString> &relativePathToExpectedSHA256);
    void verifyOriginals();

    // Call in a catch INSIDE this object's lifetime, before stack unwinding can
    // invoke best-effort cleanup. Extra evidence belongs to the caller's oracle.
    void checkpoint(const QString &label, const QJsonObject &extra = {});

    QString evidenceRoot() const;
    QString imagePath() const;
    QString mountPath() const;
    QString volumeUUID() const;
    bool attached() const;
    bool detached() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ux
