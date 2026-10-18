#include "tests/ux/HistoryChaos.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace ux {
namespace {
void check(bool condition, const QString &message) {
    if (!condition)
        throw std::runtime_error(message.toStdString());
}
QString sha(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QByteArray read(const QString &path) {
    constexpr qint64 maximum = 128LL * 1024 * 1024;
    QFile file(path);
    check(QFileInfo(path).isFile() && file.open(QIODevice::ReadOnly) && file.size() <= maximum,
          "Cannot read bounded regular chaos evidence: " + path);
    // Enforce the bound on the snapshot actually read, not only a racy pre-read
    // file size. A concurrently growing replay must never make readAll unbounded.
    const auto bytes = file.read(maximum + 1);
    check(file.error() == QFileDevice::NoError && bytes.size() <= maximum && file.atEnd(),
          "Chaos evidence grew beyond its bounded readable snapshot: " + path);
    return bytes;
}
void syncDirectory(const QString &path) {
    const auto native = QFile::encodeName(path);
    const int descriptor = ::open(native.constData(), O_RDONLY);
    check(descriptor >= 0, "Cannot open chaos evidence directory for fsync");
    const int synced = ::fsync(descriptor);
    const int closed = ::close(descriptor);
    check(synced == 0 && closed == 0, "Cannot fsync/close chaos evidence directory");
}
void durableNewFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
              file.write(bytes) == bytes.size() && file.flush() && ::fsync(file.handle()) == 0,
          "Cannot durably create new chaos evidence: " + path);
    file.close();
    syncDirectory(QFileInfo(path).absolutePath());
}
QByteArray line(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
struct SplitMix64 {
    quint64 state;
    quint64 next() {
        quint64 value = (state += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }
};
// Fixed field order and network byte order; never serialize object memory/padding,
// production preset/catalog JSON or production parameter()/merge functions.
class FullDocumentBytes {
  public:
    FullDocumentBytes() : stream_(&bytes_, QIODevice::WriteOnly) {
        stream_.setVersion(QDataStream::Qt_6_0);
        stream_.setByteOrder(QDataStream::BigEndian);
        stream_.setFloatingPointPrecision(QDataStream::DoublePrecision);
    }
    void number(double value) {
        check(std::isfinite(value), "Non-finite field in full chaos document");
        stream_ << (value == 0 ? 0.0 : value); // canonicalize equivalent +/- zero
    }
    void integer(quint64 value) {
        stream_ << value;
    }
    void boolean(bool value) {
        stream_ << quint8(value ? 1 : 0);
    }
    void text(const std::string &value) {
        check(value.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()),
              "Oversized text in full chaos document");
        integer(value.size());
        check(stream_.writeRawData(value.data(), static_cast<int>(value.size())) ==
                  static_cast<int>(value.size()),
              "Cannot encode full chaos text");
    }
    void geometry(const luma::MaskGeometry &value) {
        integer(static_cast<quint64>(value.shape));
        for (double part : {value.x1, value.y1, value.x2, value.y2, value.angle, value.feather,
                            value.rangeLow, value.rangeHigh, value.colorTolerance})
            number(part);
        for (double part : value.referenceColor)
            number(part);
        integer(value.strokes.size());
        for (const auto &stroke : value.strokes) {
            boolean(stroke.erase);
            number(stroke.radius);
            number(stroke.opacity);
            integer(stroke.points.size());
            for (const auto &point : stroke.points) {
                number(point.x);
                number(point.y);
            }
        }
    }
    QByteArray finish() {
        check(stream_.status() == QDataStream::Ok, "Cannot encode complete chaos document");
        return bytes_;
    }

  private:
    QByteArray bytes_;
    QDataStream stream_;
};
} // namespace

QString historyDocumentHash(const luma::EditDocument &d) {
    static_assert(luma::EditDocument::schemaVersion == 8,
                  "Review every field in the independent chaos hash for a new document schema");
    FullDocumentBytes out;
    out.text("luma-ux-full-typed-document-schema8-v1");
    for (double value : {d.exposure, d.temperature, d.tint})
        out.number(value);
    out.boolean(d.customWhiteBalance);
    for (double value :
         {d.contrast, d.highlights, d.shadows, d.whites, d.blacks, d.vibrance, d.saturation})
        out.number(value);
    out.integer(d.enabledCorrections);
    for (double value : {d.sharpenAmount, d.sharpenRadius, d.sharpenDetail, d.sharpenMasking,
                         d.noiseLuminance, d.noiseLuminanceDetail, d.noiseLuminanceContrast,
                         d.noiseColor, d.noiseColorDetail, d.noiseColorSmoothness})
        out.number(value);
    for (double value :
         {d.crop.x, d.crop.y, d.crop.width, d.crop.height, d.crop.angle, d.crop.aspect})
        out.number(value);
    out.integer(static_cast<quint64>(static_cast<qint64>(d.crop.quarterTurns)));
    out.boolean(d.crop.flipHorizontal);
    out.boolean(d.crop.flipVertical);
    out.text(d.lens.profile);
    out.number(d.lens.distortion);
    out.number(d.lens.vignetting);
    out.boolean(d.lens.chromaticAberration);
    for (double value :
         {d.transform.rotate, d.transform.horizontal, d.transform.vertical, d.transform.aspect,
          d.transform.scale, d.transform.xOffset, d.transform.yOffset})
        out.number(value);
    out.boolean(d.transform.constrainCrop);
    out.boolean(d.transform.guided);
    for (double value : d.transform.guide)
        out.number(value);
    for (double value : {d.curveShadows, d.curveDarks, d.curveLights, d.curveHighlights})
        out.number(value);
    for (const auto &curve : d.curves) {
        out.integer(curve.points.size());
        for (const auto &point : curve.points) {
            out.number(point.x);
            out.number(point.y);
        }
    }
    for (const auto &band : d.hsl) {
        out.number(band.hue);
        out.number(band.saturation);
        out.number(band.luminance);
    }
    out.integer(d.localMasks.size());
    for (const auto &mask : d.localMasks) {
        out.geometry(mask);
        out.text(mask.id);
        out.text(mask.name);
        out.boolean(mask.enabled);
        out.boolean(mask.inverted);
        out.number(mask.opacity);
        for (double value : mask.adjustments)
            out.number(value);
        out.integer(mask.components.size());
        for (const auto &component : mask.components) {
            out.text(component.id);
            out.integer(static_cast<quint64>(component.operation));
            out.boolean(component.enabled);
            out.boolean(component.inverted);
            out.geometry(component.geometry);
        }
    }
    return sha(out.finish());
}
void HistoryChaosModel::apply(const QString &op, const QString &control, int delta) {
    if (op == "undo") {
        check(control.isEmpty() && delta == 0, "Undo row contains edit operands");
        if (cursor_ > 0)
            --cursor_;
        return;
    }
    if (op == "redo") {
        check(control.isEmpty() && delta == 0, "Redo row contains edit operands");
        if (cursor_ + 1 < timeline_.size())
            ++cursor_;
        return;
    }
    check(op == "edit" && (delta == -1 || delta == 1), "Unsupported chaos operation/delta");
    double luma::EditDocument::*member = nullptr;
    if (control == "contrast")
        member = &luma::EditDocument::contrast;
    else if (control == "shadows")
        member = &luma::EditDocument::shadows;
    else if (control == "vibrance")
        member = &luma::EditDocument::vibrance;
    check(member, "Unsupported chaos control");
    auto next = current();
    next.*member = std::clamp(next.*member + delta, -100.0, 100.0);
    if (next == current())
        return; // no-op neither appends history nor drops Redo
    timeline_.erase(timeline_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1), timeline_.end());
    timeline_.push_back(next);
    ++cursor_;
    if (timeline_.size() > 201) {
        timeline_.pop_front();
        --cursor_;
    }
}
HistoryChaosPlan historyChaosPlan(quint64 seed, int actions, const QString &fixtures) {
    check(actions >= 1 && actions <= 100000,
          "Chaos action count must be 1..100000 (bounded evidence/resource contract)");
    HistoryChaosPlan plan;
    plan.seed = seed;
    plan.actions = actions;
    QJsonArray fixtureHashes;
    for (int index = 0; index < 10; ++index)
        fixtureHashes.append(
            sha(read(fixtures + QString("/Photo-%1.png").arg(index, 4, 10, QChar('0')))));
    HistoryChaosModel model;
    plan.header = {{"kind", "header"},
                   {"schema", 1},
                   {"case", "UX-UND-017"},
                   {"model", "schema8-timeline-cap200-v1"},
                   {"generator", "splitmix64-block64-tail400-v1"},
                   {"seed", QString::number(seed)},
                   {"actions", actions},
                   {"document_schema", 8},
                   {"application_version", QCoreApplication::applicationVersion()},
                   {"qt_version", QString::fromLatin1(qVersion())},
                   {"fixture_sha256", fixtureHashes},
                   {"initial_hash", historyDocumentHash(model.current())}};
    plan.bytes = line(plan.header);
    auto append = [&](const QString &op, const QString &control = QString{}, int delta = 0) {
        if (static_cast<int>(plan.rows.size()) == actions)
            return;
        const auto before = model.current();
        const auto beforeHash = historyDocumentHash(before);
        model.apply(op, control, delta);
        QJsonObject row{{"kind", "action"},
                        {"index", static_cast<int>(plan.rows.size()) + 1},
                        {"op", op},
                        {"control", control},
                        {"delta", delta},
                        {"before", beforeHash},
                        {"after", historyDocumentHash(model.current())},
                        {"undo", model.undoDepth()},
                        {"redo", model.redoDepth()},
                        {"changed", model.current() != before}};
        plan.rows.push_back(row);
        plan.bytes += line(row);
        // main() owns a real Metrics/QTimer during preflight, before Context.
        // Keep its actual event-loop heartbeat alive; never write fake heartbeats.
        if (plan.rows.size() % 64 == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    // 205 distinct edits guarantee a 200-entry cap; disabled shortcut inputs are
    // intentional no-ops. Then prove no-op preserves Redo and new edit drops it.
    for (int index = 0; index < 100; ++index)
        append("edit", "contrast", 1);
    for (int index = 0; index < 100; ++index)
        append("edit", "shadows", 1);
    for (int index = 0; index < 5; ++index)
        append("edit", "vibrance", 1);
    for (int index = 0; index < 201; ++index)
        append("undo");
    for (int index = 0; index < 201; ++index)
        append("redo");
    append("undo");
    append("edit", "contrast", 1); // contrast already at +100
    append("edit", "contrast", -1);
    append("redo");
    SplitMix64 random{seed};
    const QStringList controls{"contrast", "shadows", "vibrance"};
    const int randomEnd = actions >= 1011 ? actions - 400 : actions;
    int block = 0;
    QString control = controls.front();
    while (static_cast<int>(plan.rows.size()) < randomEnd) {
        if (block++ % 64 == 0)
            control = controls[static_cast<int>(random.next() % 3)];
        const auto choice = random.next() % 100;
        if (choice < 65)
            append("edit", control, random.next() % 2 ? 1 : -1);
        else
            append(choice < 83 ? "undo" : "redo");
    }
    if (actions >= 1011) {
        while (static_cast<int>(plan.rows.size()) < actions && model.undoDepth() > 0)
            append("undo");
        while (static_cast<int>(plan.rows.size()) < actions && model.redoDepth() > 0)
            append("redo");
        while (static_cast<int>(plan.rows.size()) < actions)
            append("redo"); // explicit no-op padding
    }
    check(static_cast<int>(plan.rows.size()) == actions, "Chaos generator lost configured actions");
    const auto prefix = sha(plan.bytes);
    plan.bytes += line({{"kind", "footer"},
                        {"actions", actions},
                        {"prefix_sha256", prefix},
                        {"final_hash", historyDocumentHash(model.current())},
                        {"undo", model.undoDepth()},
                        {"redo", model.redoDepth()}});
    plan.scheduleSHA256 = sha(plan.bytes);
    return plan;
}
HistoryChaosPlan prepareHistoryChaosPlan(const QString &root, const QString &fixtures, quint64 seed,
                                         int actions, const QString &replay) {
    try {
        auto plan = historyChaosPlan(seed, actions, fixtures);
        if (!replay.isEmpty()) {
            const auto supplied = read(replay);
            const auto lines = supplied.split('\n');
            check(lines.size() == actions + 3 && lines.back().isEmpty(),
                  "Replay line/action count or final newline does not match requested actions");
            QJsonParseError parse;
            const auto first = QJsonDocument::fromJson(lines.front(), &parse);
            check(parse.error == QJsonParseError::NoError && first.isObject() &&
                      first.object() == plan.header,
                  "Replay header mismatch: seed/count/model/schema/app/Qt/fixtures are immutable");
            check(supplied == plan.bytes,
                  "Replay schedule mismatch before UI: truncated/corrupt/modified or wrong "
                  "deterministic generator");
        }
        check(QDir().mkpath(root), "Cannot create chaos evidence root");
        durableNewFile(root + "/seed.txt", QString::number(seed).toUtf8() + '\n');
        durableNewFile(root + "/replay.jsonl", plan.bytes);
        return plan;
    } catch (const std::exception &error) {
        throw std::runtime_error(
            ("CHAOS seed=" + QString::number(seed) + ": " + error.what()).toStdString());
    }
}
} // namespace ux
