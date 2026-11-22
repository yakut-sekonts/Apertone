#include "apps/shared/LightroomCatalog.h"
#include "apps/shared/LightroomDevelop.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include "platform/macos/FileHasher.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <set>

namespace luma {
namespace {
using detail::Statement;

bool hasTable(sqlite3 *db, const char *name) {
    Statement record(db, "SELECT 1 FROM sqlite_schema WHERE type='table' AND name=?1");
    record.text(1, name);
    return record.row();
}
bool hasColumn(sqlite3 *db, const char *table, const char *name) {
    return sqlite3_table_column_metadata(db, nullptr, table, name, nullptr, nullptr, nullptr,
                                         nullptr, nullptr) == SQLITE_OK;
}
struct NamedParent {
    std::int64_t parent = 0;
    std::string name;
};
using NameTree = std::map<std::int64_t, NamedParent>;

NameTree readNameTree(sqlite3 *db, const char *query, CancellationToken stop) {
    NameTree tree;
    Statement node(db, query);
    while (node.row()) {
        checkCancellation(stop);
        if (tree.size() >= 200'000)
            throw std::runtime_error("Lightroom name hierarchy exceeds the migration budget.");
        tree.emplace(node.integer(0), NamedParent{node.integer(2), node.string(1)});
    }
    return tree;
}
std::string qualifyName(std::string leaf, std::int64_t parent, const NameTree &tree,
                        std::string_view separator, std::size_t limit) {
    std::set<std::int64_t> visited;
    while (parent > 0 && visited.size() < 16 && visited.insert(parent).second) {
        const auto entry = tree.find(parent);
        if (entry == tree.end())
            break;
        const auto name = QString::fromStdString(entry->second.name).simplified().toStdString();
        if (!name.empty() && name.size() + separator.size() + leaf.size() <= limit)
            leaf = name + std::string(separator) + leaf;
        parent = entry->second.parent;
    }
    return leaf;
}

std::string sourceIdentity(sqlite3 *db, const std::filesystem::path &path) {
    auto identity = QByteArray("path:") + QByteArray::fromStdString(pathUtf8(path));
    if (hasTable(db, "Adobe_variablesTable")) {
        Statement row(db, "SELECT value FROM Adobe_variablesTable "
                          "WHERE name='Adobe_storeProviderID' AND length(value)=36 LIMIT 2");
        if (row.row()) {
            const QUuid id(QString::fromStdString(row.string(0)));
            if (!id.isNull() && !row.row())
                identity = "provider:" + id.toByteArray(QUuid::WithoutBraces);
        }
    }
    return QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().toStdString();
}

struct Stamp {
    std::uintmax_t size;
    std::filesystem::file_time_type modified;
    bool operator==(const Stamp &) const = default;
};

std::optional<Stamp> stamp(const std::filesystem::path &path) {
    const auto state = std::filesystem::status(path);
    if (!std::filesystem::exists(state))
        return std::nullopt;
    if (!std::filesystem::is_regular_file(state))
        throw std::runtime_error("Lightroom catalog file is not a regular file.");
    return Stamp{std::filesystem::file_size(path), std::filesystem::last_write_time(path)};
}

void copySnapshotFile(const std::filesystem::path &source, const std::filesystem::path &target,
                      const Stamp &expected, CancellationToken stop) {
    std::ifstream in(source, std::ios::binary);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!in || !out)
        throw std::runtime_error("Cannot read Lightroom catalog snapshot.");
    std::vector<char> block(1024 * 1024);
    std::uintmax_t copied = 0;
    while (in) {
        checkCancellation(stop);
        in.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytes = in.gcount();
        copied += static_cast<std::uintmax_t>(bytes);
        if (copied > expected.size)
            throw std::runtime_error("Lightroom catalog changed during reading. Scan it again.");
        out.write(block.data(), bytes);
        if (!out)
            throw std::runtime_error("Cannot write the temporary Lightroom catalog snapshot.");
    }
    out.close();
    if (!in.eof() || !out || copied != expected.size)
        throw std::runtime_error("Lightroom catalog snapshot is incomplete.");
}

class Snapshot {
  public:
    Snapshot(const std::filesystem::path &original, CancellationToken stop)
        : temp_(QDir::tempPath() + QStringLiteral("/apertone-lr-XXXXXX")), stop_(stop) {
        if (!temp_.isValid())
            throw std::runtime_error("Cannot create the Lightroom import workspace.");
        const auto path = std::filesystem::canonical(original);
        const auto wal = pathWithSuffix(path, "-wal");
        const auto journal = pathWithSuffix(path, "-journal");
        const auto mainStamp = stamp(path), walStamp = stamp(wal);
        if (!mainStamp || mainStamp->size < 100 || stamp(journal))
            throw std::runtime_error("Close Lightroom and finish any pending catalog recovery.");
        const auto copy =
            pathFromUtf8(temp_.filePath(QStringLiteral("catalog.lrcat")).toStdString());
        const auto copyWal = pathWithSuffix(copy, "-wal");
        copySnapshotFile(path, copy, *mainStamp, stop);
        if (walStamp)
            copySnapshotFile(wal, copyWal, *walStamp, stop);
        const auto mainHash = macos::sha256File(copy, stop);
        const auto walHash = walStamp ? macos::sha256File(copyWal, stop) : std::string();
        if (mainHash != macos::sha256File(path, stop) ||
            (walStamp && walHash != macos::sha256File(wal, stop)) || stamp(path) != mainStamp ||
            stamp(wal) != walStamp || stamp(journal))
            throw std::runtime_error("Lightroom catalog changed during reading. Scan it again.");
        fingerprint = mainHash + (walStamp ? "-" + walHash : "");
        sqlite3 *db = nullptr;
        const auto flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX;
        const int opened = sqlite3_open_v2(pathUtf8(copy).c_str(), &db, flags, nullptr);
        db_.reset(db);
        if (opened != SQLITE_OK)
            throw std::runtime_error("Cannot open the Lightroom catalog database.");
        sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
        sqlite3_db_config(db, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr);
        if (sqlite3_exec(db, "PRAGMA query_only=ON; BEGIN;", nullptr, nullptr, nullptr) !=
            SQLITE_OK)
            throw std::runtime_error("Cannot prepare a read-only Lightroom import.");
        sqlite3_progress_handler(
            db, 1000,
            [](void *context) {
                return static_cast<CancellationToken *>(context)->isCancelled() ? 1 : 0;
            },
            &stop_);
        sourceKey = sourceIdentity(db, path);
    }

    sqlite3 *database() const {
        return db_.get();
    }
    std::string fingerprint, sourceKey;

  private:
    QTemporaryDir temp_;
    CancellationToken stop_;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_{nullptr, sqlite3_close};
};

void requireTable(sqlite3 *db, const char *name) {
    if (!hasTable(db, name))
        throw std::runtime_error("Unsupported Lightroom Classic catalog structure.");
}

// Original paths come from the selected catalog. Keep drive-relative paths,
// traversal and Windows device/network names out of filesystem probes.
std::filesystem::path originalPath(const std::string &root, const std::string &folder,
                                   const std::string &basename, const std::string &extension) {
    if (root.empty() || root.size() > 4096 || folder.size() > 4096 || basename.empty() ||
        basename.size() > 1024 || extension.empty() || extension.size() > 16 || basename == "." ||
        basename == ".." || basename.find_first_of("/\\:") != std::string::npos ||
        extension.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos)
        return {};
    auto base = pathFromUtf8(root);
    const auto relative = pathFromUtf8(folder);
    if (!base.is_absolute() || relative.has_root_path())
        return {};
#ifdef _WIN32
    const auto drive = base.root_name().native();
    if (drive.size() != 2 || drive[1] != L':')
        return {};
#else
    if (root.starts_with("//"))
        return {};
#endif
    for (const auto &component : relative)
        if (component == "..")
            return {};
    return (base / relative / pathFromUtf8(basename + "." + extension)).lexically_normal();
}

QString displayName(const std::string &value, const QString &fallback) {
    auto result = QString::fromStdString(value).simplified();
    for (auto &ch : result)
        if (ch.category() == QChar::Other_Control)
            ch = QChar(' ');
    return result.isEmpty() ? fallback : result.left(120);
}

bool sourceID(std::string_view value) {
    return value.size() == 36 && value.find_first_not_of("0123456789abcdefABCDEF-") == value.npos;
}

std::string metadataText(const std::string &value, bool multiline = false) {
    auto text = QString::fromStdString(value);
    for (auto &character : text) {
        const auto ch = character.unicode();
        if (ch < 32 && !(multiline && (ch == '\r' || ch == '\n' || ch == '\t')))
            character = QChar(' ');
    }
    return text.toStdString();
}

bool samePath(const std::filesystem::path &left, const std::filesystem::path &right) {
#ifdef _WIN32
    const auto normalized = [](const std::filesystem::path &path) {
        return QDir::fromNativeSeparators(QString::fromStdWString(path.native()));
    };
    if (normalized(left).compare(normalized(right), Qt::CaseInsensitive) == 0)
        return true;
#else
    if (left == right)
        return true;
#endif
    std::error_code error;
    return std::filesystem::equivalent(left, right, error);
}

std::map<std::string, DescriptiveMetadata> catalogDescriptions(sqlite3 *db,
                                                               CancellationToken stop) {
    std::map<std::string, DescriptiveMetadata> values;
    if (hasTable(db, "AgLibraryIPTC")) {
        Statement rows(db, "SELECT i.id_global,"
                           "CASE WHEN length(CAST(t.caption AS BLOB))<=16384 THEN t.caption END,"
                           "CASE WHEN length(CAST(t.copyright AS BLOB))<=2048 THEN t.copyright END "
                           "FROM AgLibraryIPTC t JOIN Adobe_images i ON i.id_local=t.image "
                           "WHERE coalesce(t.caption,'')<>'' OR coalesce(t.copyright,'')<>'' "
                           "ORDER BY t.id_local");
        while (rows.row()) {
            checkCancellation(stop);
            auto &description = values[rows.string(0)];
            description.caption = metadataText(rows.string(1), true);
            description.copyright = metadataText(rows.string(2));
        }
    }
    if (hasTable(db, "AgHarvestedIptcMetadata") && hasTable(db, "AgInternedIptcCreator") &&
        hasTable(db, "AgInternedIptcLocation")) {
        Statement rows(db,
                       "SELECT i.id_global,"
                       "CASE WHEN length(CAST(c.value AS BLOB))<=1024 THEN c.value END,"
                       "CASE WHEN length(CAST(l.value AS BLOB))<=2048 THEN l.value END "
                       "FROM AgHarvestedIptcMetadata t JOIN Adobe_images i ON i.id_local=t.image "
                       "LEFT JOIN AgInternedIptcCreator c ON c.id_local=t.creatorRef "
                       "LEFT JOIN AgInternedIptcLocation l ON l.id_local=t.locationRef "
                       "WHERE c.value IS NOT NULL OR l.value IS NOT NULL ORDER BY t.id_local");
        while (rows.row()) {
            checkCancellation(stop);
            auto &description = values[rows.string(0)];
            const auto creator = metadataText(rows.string(1));
            if (!creator.empty() && description.creators.size() < 256)
                description.creators.push_back(creator);
            if (const auto location = metadataText(rows.string(2)); !location.empty())
                description.location = location;
        }
    }
    return values;
}

struct Scan {
    LightroomCatalogReport report;
    std::vector<LightroomMigrationPhoto> photos;
    std::vector<LightroomMigrationCollection> collections;
};

Scan scan(Snapshot &snapshot, bool prepare, CancellationToken stop,
          const std::function<void(double)> &progress = {}) {
    auto *db = snapshot.database();
    for (const char *table :
         {"Adobe_images", "AgLibraryFile", "AgLibraryFolder", "AgLibraryRootFolder",
          "Adobe_imageDevelopSettings", "AgLibraryCollection", "AgLibraryCollectionImage"})
        requireTable(db, table);
    Scan result;
    auto &report = result.report;
    report.fingerprint = snapshot.fingerprint;
    {
        Statement count(db, "SELECT COUNT(*) FROM Adobe_images");
        if (!count.row())
            throw std::runtime_error("Cannot count Lightroom photos.");
        report.photos = static_cast<std::size_t>(count.integer(0));
    }

    auto descriptions =
        prepare ? catalogDescriptions(db, stop) : std::map<std::string, DescriptiveMetadata>{};
    if (prepare) {
        requireTable(db, "AgLibraryKeyword");
        requireTable(db, "AgLibraryKeywordImage");
        const bool keywordParents = hasColumn(db, "AgLibraryKeyword", "parent");
        const auto keywordTree =
            keywordParents ? readNameTree(db,
                                          "SELECT id_local,name,parent FROM AgLibraryKeyword "
                                          "WHERE length(CAST(name AS BLOB))<=512 ORDER BY id_local",
                                          stop)
                           : NameTree{};
        const auto keywordQuery =
            std::string("SELECT i.id_global,k.name,k.id_local,") +
            (keywordParents ? "k.parent" : "NULL") +
            " FROM AgLibraryKeywordImage m "
            "JOIN AgLibraryKeyword k ON k.id_local=m.tag "
            "JOIN Adobe_images i ON i.id_local=m.image "
            "WHERE length(CAST(k.name AS BLOB)) BETWEEN 1 AND 512 ORDER BY m.id_local";
        Statement tags(db, keywordQuery.c_str());
        std::map<std::int64_t, std::vector<std::string>> synonyms;
        if (hasTable(db, "AgLibraryKeywordSynonym") &&
            hasColumn(db, "AgLibraryKeywordSynonym", "name")) {
            const char *owner = hasColumn(db, "AgLibraryKeywordSynonym", "keyword") ? "keyword"
                                : hasColumn(db, "AgLibraryKeywordSynonym", "tag")   ? "tag"
                                                                                    : nullptr;
            if (owner) {
                Statement aliases(db, (std::string("SELECT ") + owner +
                                       ",name FROM AgLibraryKeywordSynonym "
                                       "WHERE length(CAST(name AS BLOB)) BETWEEN 1 AND 512")
                                          .c_str());
                std::size_t aliasCount = 0;
                while (aliases.row()) {
                    checkCancellation(stop);
                    if (++aliasCount > 200'000)
                        throw std::runtime_error(
                            "Lightroom keyword aliases exceed the migration budget.");
                    auto &entries = synonyms[aliases.integer(0)];
                    if (entries.size() < 64)
                        entries.push_back(metadataText(aliases.string(1)));
                }
            }
        }
        while (tags.row()) {
            checkCancellation(stop);
            const auto keyword = metadataText(tags.string(1));
            if (keyword.size() > 512 || keyword.find_first_not_of(' ') == keyword.npos)
                continue;
            auto &values = descriptions[tags.string(0)].keywords;
            const auto append = [&](const std::string &text) {
                if (!text.empty() && text.size() <= 512 && values.size() < 256 &&
                    std::ranges::find(values, text) == values.end())
                    values.push_back(text);
            };
            append(keyword);
            append(qualifyName(keyword, tags.integer(3), keywordTree, "|", 512));
            if (const auto alias = synonyms.find(tags.integer(2)); alias != synonyms.end())
                for (const auto &value : alias->second)
                    append(value);
        }
    }

    const auto capture = hasColumn(db, "Adobe_images", "captureTime") ? "i.captureTime" : "NULL";
    const auto edited = hasColumn(db, "Adobe_images", "touchTime") ? "i.touchTime" : "NULL";
    const auto imageQuery =
        std::string("SELECT i.id_global,master.id_global,i.copyName,i.rating,i.pick,"
                    "i.colorLabels,i.fileFormat,root.absolutePath,folder.pathFromRoot,"
                    "f.baseName,f.extension,CASE WHEN length(CAST(s.text AS BLOB))<=1048576 "
                    "THEN s.text ELSE NULL END,COALESCE(length(CAST(s.text AS BLOB)),0),") +
        capture + "," + edited +
        " "
        "FROM Adobe_images i LEFT JOIN AgLibraryFile f ON f.id_local=i.rootFile "
        "LEFT JOIN AgLibraryFolder folder ON folder.id_local=f.folder "
        "LEFT JOIN AgLibraryRootFolder root ON root.id_local=folder.rootFolder "
        "LEFT JOIN Adobe_images master ON master.id_local=i.masterImage "
        "LEFT JOIN Adobe_imageDevelopSettings s ON s.image=i.id_local "
        "ORDER BY COALESCE(i.masterImage,0)<>0,i.id_local";
    Statement images(db, imageQuery.c_str());
    struct Original {
        std::string digest;
        PhotoMetadata metadata;
        Stamp stamp;
    };
    std::map<std::filesystem::path, Original> inspected;
    std::set<std::string> seen;
    std::size_t completed = 0;
    while (images.row()) {
        checkCancellation(stop);
        ++completed;
        if (progress && (completed % 8 == 0 || completed == report.photos))
            progress(0.75 * static_cast<double>(completed) /
                     std::max<std::size_t>(1, report.photos));
        const auto id = images.string(0), master = images.string(1);
        if (!sourceID(id) || !seen.insert(id).second)
            throw std::runtime_error("Invalid or duplicate photo identifier in Lightroom catalog.");
        report.virtualCopies += !master.empty();
        const auto path =
            originalPath(images.string(7), images.string(8), images.string(9), images.string(10));
        if (path.empty() || images.string(6) == "VIDEO" ||
            !PhotoDecoder::supportedExtension(path)) {
            ++report.unsupported;
            continue;
        }
        std::error_code fsError;
        if (!std::filesystem::is_regular_file(path, fsError)) {
            ++report.missing;
            if (report.warnings.size() < 20)
                report.warnings.push_back("Original unavailable: " + pathUtf8(path));
            continue;
        }
        ++report.available;
        LightroomMigrationPhoto photo;
        photo.sourceID = id;
        photo.masterID = master;
        photo.copyName =
            master.empty() ? "" : displayName(images.string(2), "Virtual copy").toStdString();
        photo.developDocument = images.string(11);
        if (images.integer(12) > 0) {
            try {
                const auto edit = decodeLightroomDevelop(photo.developDocument);
                photo.edits = mergeSettings(EditDocument{}, edit.preset.settings,
                                            edit.preset.groups, edit.preset.parameters);
                ++report.mappedEdits;
                report.partialEdits += !edit.unsupported.isEmpty();
            } catch (const std::exception &) {
                ++report.unmappedEdits;
            }
        }
        if (!prepare)
            continue;
        try {
            photo.path = std::filesystem::canonical(path);
            auto found = inspected.find(photo.path);
            if (found == inspected.end()) {
                const auto before = stamp(photo.path);
                if (!before || before->size > 1'300'000'000)
                    throw std::runtime_error("Original exceeds the input budget or disappeared.");
                const auto digest = macos::sha256File(photo.path, stop);
                auto metadata = PhotoDecoder().inspect(photo.path, stop);
                if (stamp(photo.path) != before)
                    throw std::runtime_error("Original changed while reading metadata.");
                found =
                    inspected.emplace(photo.path, Original{digest, std::move(metadata), *before})
                        .first;
            }
            photo.checksum = found->second.digest;
            photo.metadata = found->second.metadata;
            auto captured =
                QDateTime::fromString(QString::fromStdString(images.string(13)), Qt::ISODateWithMs);
            if (captured.isValid()) {
                if (captured.timeSpec() == Qt::LocalTime)
                    captured = QDateTime(captured.date(), captured.time(), QTimeZone::UTC);
                photo.metadata.captureTimestamp = captured.toSecsSinceEpoch();
            }
            if (const auto editTime = images.optional(14); editTime && std::isfinite(*editTime) &&
                                                           *editTime >= 0 &&
                                                           *editTime < 3'000'000'000.0)
                photo.editedAt =
                    *editTime > 0 ? 978307200 + static_cast<std::int64_t>(*editTime) : 0;
            auto description = photo.metadata.descriptive.value_or(DescriptiveMetadata{});
            description.rating =
                static_cast<int>(std::clamp<std::int64_t>(images.integer(3), 0, 5));
            description.flag = static_cast<int>(std::clamp<std::int64_t>(images.integer(4), -1, 1));
            description.label = displayName(images.string(5), "").toStdString();
            if (const auto stored = descriptions.find(id); stored != descriptions.end()) {
                const auto &catalog = stored->second;
                if (!catalog.caption.empty())
                    description.caption = catalog.caption;
                if (!catalog.copyright.empty())
                    description.copyright = catalog.copyright;
                if (!catalog.location.empty())
                    description.location = catalog.location;
                if (!catalog.creators.empty())
                    description.creators = catalog.creators;
                if (!catalog.keywords.empty())
                    description.keywords = catalog.keywords;
            }
            description.validate();
            photo.metadata.descriptive = std::move(description);
            photo.metadata.descriptiveSource = photo.path;
            result.photos.push_back(std::move(photo));
        } catch (const std::exception &error) {
            checkCancellation(stop);
            ++report.unreadable;
            if (report.warnings.size() < 20)
                report.warnings.push_back(pathUtf8(path) + " · " + error.what());
        }
    }
    for (const auto &[path, original] : inspected)
        if (stamp(path) != original.stamp)
            throw std::runtime_error("Original changed while planning Lightroom import.");

    const bool collectionParents = hasColumn(db, "AgLibraryCollection", "parent");
    const auto collectionTree =
        collectionParents ? readNameTree(db,
                                         "SELECT id_local,name,parent FROM AgLibraryCollection "
                                         "WHERE length(CAST(name AS BLOB))<=512 ORDER BY id_local",
                                         stop)
                          : NameTree{};
    Statement collections(
        db,
        collectionParents
            ? "SELECT id_local,name,creationId,parent FROM AgLibraryCollection ORDER BY id_local"
            : "SELECT id_local,name,creationId,NULL FROM AgLibraryCollection ORDER BY id_local");
    const bool hasRules = hasTable(db, "AgLibraryCollectionContent");
    while (collections.row()) {
        checkCancellation(stop);
        const auto type = collections.string(2);
        const bool smart = type == "com.adobe.ag.library.smart_collection";
        if (!smart && type != "com.adobe.ag.library.collection")
            continue;
        report.smartCollections += smart;
        report.manualCollections += !smart;
        if (!prepare && !smart)
            continue;
        LightroomMigrationCollection collection;
        collection.sourceID = collections.integer(0);
        auto name = displayName(collections.string(1), "Collection");
        while (name.toUtf8().size() > 96)
            name.chop(1);
        collection.name =
            qualifyName(name.toStdString(), collections.integer(3), collectionTree, " › ", 96);
        collection.smart = smart;
        if (smart) {
            if (hasRules) {
                Statement rule(db,
                               "SELECT content FROM AgLibraryCollectionContent "
                               "WHERE collection=?1 AND owningModule='ag.library.smart_collection' "
                               "AND length(CAST(content AS BLOB)) BETWEEN 1 AND 65536 "
                               "ORDER BY id_local LIMIT 2");
                rule.integer(1, collection.sourceID);
                if (rule.row()) {
                    collection.sourceRule = rule.string(0);
                    if (!rule.row()) {
                        try {
                            collection.rule = decodeLightroomSmartRule(collection.sourceRule);
                        } catch (const std::exception &) {
                            // Preserve the source document when its rule has no equivalent.
                        }
                    }
                }
            }
            report.smartRulesMapped += collection.rule.has_value();
            if (!collection.rule && report.warnings.size() < 20)
                report.warnings.push_back("Unsupported Smart Collection: " + collection.name);
            if (prepare)
                result.collections.push_back(std::move(collection));
            continue;
        }
        Statement members(db, "SELECT i.id_global FROM AgLibraryCollectionImage m "
                              "JOIN Adobe_images i ON i.id_local=m.image "
                              "WHERE m.collection=?1 ORDER BY m.positionInCollection,m.id_local");
        members.integer(1, collection.sourceID);
        while (members.row())
            collection.members.push_back(members.string(0));
        result.collections.push_back(std::move(collection));
    }
    return result;
}
} // namespace

LightroomCatalogReport inspectLightroomCatalog(const std::filesystem::path &source,
                                               CancellationToken stop) {
    Snapshot snapshot(source, stop);
    return scan(snapshot, false, stop).report;
}

LightroomCatalogReport importLightroomCatalog(const std::filesystem::path &source,
                                              const std::filesystem::path &target,
                                              const std::string &expectedFingerprint,
                                              CancellationToken stop,
                                              const std::function<void(double)> &progress) {
    const auto input = std::filesystem::canonical(source);
    const auto destination = std::filesystem::weakly_canonical(target);
    for (const auto suffix : {"", "-wal", "-shm", "-journal"}) {
        if (samePath(pathWithSuffix(input, suffix), destination))
            throw std::runtime_error("Choose an Apertone catalog for the migration destination.");
    }
    Snapshot snapshot(input, stop);
    if (!expectedFingerprint.empty() && expectedFingerprint != snapshot.fingerprint)
        throw std::runtime_error("Lightroom catalog changed since preview. Scan it again.");
    auto plan = scan(snapshot, true, stop, progress);
    checkCancellation(stop);
    if (plan.photos.empty() && plan.collections.empty())
        return std::move(plan.report);
    plan.report.imported = Catalog(target).applyLightroomMigration(snapshot.sourceKey, plan.photos,
                                                                   plan.collections, stop);
    if (progress)
        progress(1);
    return std::move(plan.report);
}
} // namespace luma
