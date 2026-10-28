#include "core/ai/Culling.h"
#include "core/catalog/Catalog.h"
#include "platform/macos/PhotoCulling.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <unistd.h>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F operation) {
    try {
        operation();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Invalid culling operation was accepted.");
}
struct Temporary {
    std::filesystem::path root;
    Temporary() {
        auto pattern =
            (std::filesystem::temp_directory_path() / "apertone-culling-XXXXXX").string();
        require(mkdtemp(pattern.data()) != nullptr, "Cannot create culling fixture.");
        root = pattern;
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

void sql(const std::filesystem::path &path, const char *script) {
    sqlite3 *database = nullptr;
    require(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "Cannot open culling fixture.");
    char *error = nullptr;
    const auto result = sqlite3_exec(database, script, nullptr, nullptr, &error);
    const std::string message = error ? error : "SQLite fixture update failed.";
    sqlite3_free(error);
    sqlite3_close(database);
    if (result != SQLITE_OK)
        throw std::runtime_error(message);
}

void setChecksum(const std::filesystem::path &path, const std::string &id,
                 const std::string &checksum) {
    sqlite3 *database = nullptr;
    require(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "Cannot update culling fixture.");
    sqlite3_stmt *statement = nullptr;
    require(sqlite3_prepare_v2(database, "UPDATE assets SET checksum=? WHERE id=?", -1, &statement,
                               nullptr) == SQLITE_OK,
            "Cannot prepare checksum update.");
    sqlite3_bind_text(statement, 1, checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    const auto result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    sqlite3_close(database);
    require(result == SQLITE_DONE, "Cannot update fixture checksum.");
}

std::vector<std::uint8_t> preview(bool detailed) {
    std::vector<std::uint8_t> result(64 * 64 * 4, 255);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const auto value = detailed && ((x / 2 + y / 2) % 2) ? 255 : 0;
            const auto offset = (static_cast<std::size_t>(y) * 64 + x) * 4;
            result[offset] = result[offset + 1] = result[offset + 2] =
                static_cast<std::uint8_t>(value);
        }
    return result;
}

void scoreChecks() {
    auto flat = preview(false), detailed = preview(true);
    const auto rejected = luma::ai::scoreCullingPreview({64, 64, flat}, {1, 0}, {});
    const auto picked = luma::ai::scoreCullingPreview({64, 64, detailed}, {1, 0}, {});
    require(rejected.recommendation == luma::ai::CullingRecommendation::reject &&
                (rejected.reasons & luma::ai::softFocus) &&
                (rejected.reasons & luma::ai::exposureIssue),
            "Flat underexposed preview was not rejected with explicit reasons.");
    require(picked.sharpness > rejected.sharpness && picked.score > rejected.score,
            "Detailed preview did not receive the stronger quality score.");
    luma::ai::FaceObservation closed{{.1, .1, .4, .4}, .7, .05, {1, 0}};
    luma::ai::FaceObservation open{{.55, .1, .35, .4}, .9, .95, {0, 1}};
    const auto portrait = luma::ai::scoreCullingPreview({64, 64, detailed}, {1, 0}, {open, closed});
    require(portrait.recommendation == luma::ai::CullingRecommendation::reject &&
                (portrait.reasons & luma::ai::closedEyes) && portrait.eyesOpen == .05,
            "One closed-eye face in a group did not force an explainable reject suggestion.");
    luma::CancellationSource cancelled;
    cancelled.cancel();
    rejects(
        [&] { luma::ai::scoreCullingPreview({64, 64, detailed}, {1, 0}, {}, cancelled.token()); });
    auto invalid = picked;
    invalid.score = 2;
    rejects([&] { invalid.validate(); });
}

void providerChecks() {
    auto pixels = preview(true);
    const auto provider = luma::macos::makePhotoCulling();
    require(provider->location() == luma::ai::ExecutionLocation::local &&
                provider->identifier().starts_with("apertone.culling-v1."),
            "Culling provider is not a versioned local implementation.");
    double lastProgress = 0;
    const auto result = provider->analyze({64, 64, pixels}, {}, [&](double progress) {
        require(progress >= lastProgress && progress >= 0 && progress <= 1,
                "Culling provider progress is invalid.");
        lastProgress = progress;
    });
    result.validate();
    require(lastProgress == 1 && result.imageEmbedding.size() == 261,
            "Native perceptual descriptor contract changed unexpectedly.");
}

luma::ai::CullingObservation observation(double score, std::vector<float> image,
                                         std::vector<float> face) {
    luma::ai::CullingObservation result;
    result.sharpness = score;
    result.exposure = .8;
    result.faceQuality = .8;
    result.eyesOpen = .8;
    result.score = score;
    result.recommendation = score >= .72 ? luma::ai::CullingRecommendation::pick
                                         : luma::ai::CullingRecommendation::review;
    result.imageEmbedding = std::move(image);
    result.faces.push_back({{.1, .1, .3, .3}, .8, .8, std::move(face)});
    result.validate();
    return result;
}

void catalogChecks() {
    Temporary temporary;
    const auto path = temporary.root / "catalog.sqlite";
    luma::Catalog catalog(path);
    std::vector<std::string> ids;
    for (int index = 0; index < 3; ++index) {
        const auto source = temporary.root / ("burst-" + std::to_string(index) + ".jpg");
        std::ofstream(source) << "fixture " << index;
        luma::PhotoMetadata metadata;
        metadata.width = 100;
        metadata.height = 80;
        metadata.camera = "Apertone test camera";
        metadata.captureTimestamp = 1000 + index;
        ids.push_back(catalog.addAndSelect(source, std::string(64, 'a' + index), metadata).id);
    }
    constexpr auto provider = "test.local-culling-v1";
    const std::vector<std::vector<float>> imageVectors{{1, 0, 0}, {.99F, .1F, 0}, {.98F, 0, .1F}};
    const std::vector<std::vector<float>> faceVectors{{1, 0, 0}, {.99F, .05F, 0}, {0, 1, 0}};
    for (int index = 0; index < 3; ++index)
        catalog.saveCullingAnalysis(
            ids[index], std::string(64, 'a' + index), provider,
            observation(.70 + index * .1, imageVectors[index], faceVectors[index]));
    require(catalog.pendingCullingImages(provider).empty(),
            "Completed culling work remained pending.");
    catalog.rebuildIntelligentGroups(provider);
    const auto summary = catalog.cullingSummary();
    require(summary.analyzed == 3 && summary.bursts == 1 && summary.faceGroups == 2 &&
                summary.picks == 1 && summary.reviews == 2,
            "Intelligent grouping summary is inconsistent.");
    require(catalog.cullingIDs(1).size() == 1 && catalog.cullingIDs(-2, {}, true).size() == 1,
            "Best-of-burst or Pick selection is incorrect.");

    const auto groups = catalog.faceGroups();
    const auto pair = std::find_if(groups.begin(), groups.end(),
                                   [](const auto &group) { return group.count == 2; });
    require(pair != groups.end(), "Similar faces were not grouped together.");
    luma::LibraryFilter groupFilter;
    groupFilter.faceGroupID = pair->id;
    require(catalog.libraryCount(groupFilter) == 2,
            "Face-group filter did not select the grouped photos.");
    catalog.renameFaceGroup(pair->id, "Alex");
    catalog.saveCullingAnalysis(ids[0], std::string(64, 'a'), provider,
                                observation(.75, imageVectors[0], faceVectors[0]));
    catalog.rebuildIntelligentGroups(provider);
    const auto renamed = catalog.faceGroups();
    require(std::any_of(renamed.begin(), renamed.end(),
                        [](const auto &group) { return group.name == "Alex" && group.count == 2; }),
            "Named face group was not preserved across local reanalysis.");

    const auto beforeCancellation = catalog.cullingSummary();
    luma::CancellationSource cancelled;
    cancelled.cancel();
    rejects([&] { catalog.rebuildIntelligentGroups(provider, cancelled.token()); });
    const auto afterCancellation = catalog.cullingSummary();
    require(afterCancellation.analyzed == beforeCancellation.analyzed &&
                afterCancellation.bursts == beforeCancellation.bursts &&
                afterCancellation.faceGroups == beforeCancellation.faceGroups,
            "Cancelled grouping published a partial index.");

    for (const auto &id : ids)
        setChecksum(path, id, "stale");
    require(catalog.cullingSummary().analyzed == 0 && catalog.cullingIDs().empty() &&
                catalog.libraryCount(groupFilter) == 0 && catalog.faceGroups().empty(),
            "Stale local analysis remained visible after an asset checksum changed.");
    for (int index = 0; index < 3; ++index)
        setChecksum(path, ids[index], std::string(64, 'a' + index));

    catalog.setFlag({ids[0]}, -1);
    require(catalog.applyCullingSuggestions(catalog.cullingIDs()) == 2,
            "Suggestions overwrote a manual flag or skipped unflagged photos.");
    require(catalog.applyCullingSuggestions({}) == 0,
            "An empty suggestion scope unexpectedly changed the whole catalog.");
    const auto page = catalog.libraryPage({}, 0, 10);
    const auto manual =
        std::find_if(page.begin(), page.end(), [&](const auto &item) { return item.id == ids[0]; });
    require(manual != page.end() && manual->flag == -1,
            "Applying suggestions overwrote a manual Reject flag.");

    // A provider revision must not silently erase user-authored group names.
    constexpr auto nextProvider = "test.local-culling-v2";
    rejects([&] {
        catalog.saveCullingAnalysis(ids[0], std::string(64, 'a'), nextProvider,
                                    observation(.65, imageVectors[0], faceVectors[0]));
    });
    const auto preservedGroups = catalog.faceGroups();
    require(std::any_of(preservedGroups.begin(), preservedGroups.end(),
                        [](const auto &group) { return group.name == "Alex"; }),
            "Provider mismatch erased a named face group.");
    catalog.clearCullingIndex();
    for (int index = 0; index < 3; ++index)
        catalog.saveCullingAnalysis(
            ids[index], std::string(64, 'a' + index), nextProvider,
            observation(.65 + index * .1, imageVectors[index], faceVectors[index]));
    catalog.rebuildIntelligentGroups(nextProvider);
    require(catalog.cullingSummary().bursts == 1,
            "Provider upgrade left conflicting burst membership.");
    catalog.clearCullingIndex();
    require(catalog.cullingSummary().analyzed == 0 && catalog.faceGroups().empty(),
            "Clearing local analysis left derived groups behind.");

    sqlite3 *raw = nullptr;
    require(sqlite3_open(path.c_str(), &raw) == SQLITE_OK, "Cannot inspect culling schema.");
    sqlite3_stmt *statement = nullptr;
    require(sqlite3_prepare_v2(raw, "PRAGMA user_version", -1, &statement, nullptr) == SQLITE_OK &&
                sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 22,
            "Catalog did not migrate to Intelligent Library schema 22.");
    sqlite3_finalize(statement);
    sqlite3_close(raw);
}

void migrationChecks() {
    Temporary temporary;
    const auto path = temporary.root / "catalog.sqlite";
    {
        luma::Catalog current(path);
    }
    sql(path, "DROP TABLE burst_members;DROP TABLE burst_stacks;DROP TABLE face_instances;"
              "DROP TABLE face_groups;DROP TABLE culling_analysis;"
              "DELETE FROM schema_migrations WHERE version=22;PRAGMA user_version=21;");
    {
        luma::Catalog migrated(path, luma::CatalogOpenMode::existingOnly);
        require(migrated.cullingSummary().analyzed == 0,
                "Schema-21 catalog did not gain Intelligent Library tables.");
    }
    sqlite3 *database = nullptr;
    require(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "Cannot inspect migrated catalog.");
    sqlite3_stmt *version = nullptr;
    require(sqlite3_prepare_v2(database, "PRAGMA user_version", -1, &version, nullptr) ==
                    SQLITE_OK &&
                sqlite3_step(version) == SQLITE_ROW && sqlite3_column_int(version, 0) == 22,
            "Schema-21 catalog did not migrate to schema 22.");
    sqlite3_finalize(version);
    sqlite3_close(database);
    const auto backups = temporary.root / "catalog.sqlite.luma-backups";
    require(std::filesystem::is_directory(backups) &&
                std::distance(std::filesystem::directory_iterator(backups),
                              std::filesystem::directory_iterator{}) == 1,
            "Schema-21 migration did not preserve a pre-migration backup.");
}
} // namespace

int main() {
    try {
        scoreChecks();
        providerChecks();
        migrationChecks();
        catalogChecks();
        std::cout << "Local culling, similar-face groups and Best of Burst contracts passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
