#include "core/ai/Embedding.h"
#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/catalog/SqliteStatement.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>

namespace luma {
namespace {
using detail::Statement;

std::vector<std::uint8_t> encodeVector(std::span<const float> input) {
    const auto values = ai::normalizedEmbedding(input);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 4);
    for (const float value : values) {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (int byte = 0; byte < 4; ++byte)
            bytes.push_back(static_cast<std::uint8_t>(bits >> (8 * byte)));
    }
    return bytes;
}

std::vector<float> decodeVector(const Statement &query, int blobColumn, int dimensionColumn) {
    const auto dimension = query.integer(dimensionColumn);
    const auto bytes = query.bytes(blobColumn);
    if (dimension < 1 || dimension > 2048 ||
        bytes.size() != static_cast<std::size_t>(dimension) * 4)
        throw std::runtime_error("Invalid stored culling feature print.");
    std::vector<float> values(static_cast<std::size_t>(dimension));
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::uint32_t bits = 0;
        for (int byte = 0; byte < 4; ++byte)
            bits |= static_cast<std::uint32_t>(bytes[i * 4 + byte]) << (8 * byte);
        values[i] = std::bit_cast<float>(bits);
    }
    return ai::normalizedEmbedding(values);
}

double similarity(std::span<const float> first, std::span<const float> second) {
    if (first.size() != second.size() || first.empty())
        return -1;
    double result = 0;
    for (std::size_t i = 0; i < first.size(); ++i)
        result += static_cast<double>(first[i]) * second[i];
    return result;
}

void validateProvider(const std::string &provider) {
    if (provider.empty() || provider.size() > 256 || provider.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid culling provider identity.");
}

void requireChanged(sqlite3 *database, const char *message) {
    if (sqlite3_changes(database) != 1)
        throw std::runtime_error(message);
}
} // namespace

void Catalog::migrateIntelligentLibrary() {
    execute(R"sql(
        CREATE TABLE culling_analysis(
            asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
            checksum TEXT NOT NULL,provider TEXT NOT NULL CHECK(length(provider) BETWEEN 1 AND 256),
            sharpness REAL NOT NULL CHECK(sharpness BETWEEN 0 AND 1),
            exposure REAL NOT NULL CHECK(exposure BETWEEN 0 AND 1),
            face_quality REAL NOT NULL CHECK(face_quality BETWEEN 0 AND 1),
            eyes_open REAL NOT NULL CHECK(eyes_open BETWEEN 0 AND 1),
            score REAL NOT NULL CHECK(score BETWEEN 0 AND 1),
            base_recommendation INTEGER NOT NULL CHECK(base_recommendation IN (-1,0,1)),
            recommendation INTEGER NOT NULL CHECK(recommendation IN (-1,0,1)),
            reason_flags INTEGER NOT NULL CHECK(reason_flags BETWEEN 0 AND 15),
            dimension INTEGER NOT NULL CHECK(dimension BETWEEN 1 AND 2048),
            embedding BLOB NOT NULL CHECK(typeof(embedding)='blob' AND length(embedding)=4*dimension),
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
        CREATE INDEX culling_recommendation ON culling_analysis(recommendation,score,asset_id);
        CREATE TABLE people(
            id TEXT PRIMARY KEY,provider TEXT NOT NULL CHECK(length(provider) BETWEEN 1 AND 256),
            name TEXT NOT NULL DEFAULT '' CHECK(length(name)<=128),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
        CREATE TABLE face_instances(
            id TEXT PRIMARY KEY,asset_id TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
            provider TEXT NOT NULL CHECK(length(provider) BETWEEN 1 AND 256),
            face_index INTEGER NOT NULL CHECK(face_index BETWEEN 0 AND 63),
            x REAL NOT NULL CHECK(x BETWEEN 0 AND 1),y REAL NOT NULL CHECK(y BETWEEN 0 AND 1),
            width REAL NOT NULL CHECK(width>0 AND width<=1 AND x+width<=1.000001),
            height REAL NOT NULL CHECK(height>0 AND height<=1 AND y+height<=1.000001),
            quality REAL NOT NULL CHECK(quality BETWEEN 0 AND 1),
            eyes_open REAL NOT NULL CHECK(eyes_open BETWEEN 0 AND 1),
            dimension INTEGER NOT NULL CHECK(dimension BETWEEN 1 AND 2048),
            embedding BLOB NOT NULL CHECK(typeof(embedding)='blob' AND length(embedding)=4*dimension),
            person_id TEXT REFERENCES people(id) ON DELETE SET NULL,
            UNIQUE(asset_id,provider,face_index));
        CREATE INDEX face_person ON face_instances(person_id,asset_id);
        CREATE TABLE burst_stacks(
            id TEXT PRIMARY KEY,provider TEXT NOT NULL CHECK(length(provider) BETWEEN 1 AND 256),
            started_at INTEGER NOT NULL);
        CREATE TABLE burst_members(
            stack_id TEXT NOT NULL REFERENCES burst_stacks(id) ON DELETE CASCADE,
            asset_id TEXT NOT NULL UNIQUE REFERENCES assets(id) ON DELETE CASCADE,
            position INTEGER NOT NULL CHECK(position>=0),
            is_best INTEGER NOT NULL CHECK(is_best IN (0,1)),PRIMARY KEY(stack_id,asset_id));
        CREATE INDEX burst_member_order ON burst_members(stack_id,position);
        CREATE UNIQUE INDEX burst_position_unique ON burst_members(stack_id,position);
        CREATE UNIQUE INDEX burst_best_unique ON burst_members(stack_id) WHERE is_best=1;
        INSERT INTO schema_migrations(version) VALUES(22);
        PRAGMA user_version=22;
    )sql");
}

std::vector<std::string> Catalog::pendingCullingImages(const std::string &provider,
                                                       const std::vector<std::string> &ids) const {
    validateProvider(provider);
    std::vector<std::string> result;
    if (ids.empty()) {
        Statement query(db_, "SELECT a.id FROM assets a WHERE NOT EXISTS(SELECT 1 FROM "
                             "culling_analysis c WHERE c.asset_id=a.id AND c.provider=?1 AND "
                             "c.checksum=a.checksum) ORDER BY a.id");
        query.text(1, provider);
        while (query.row())
            result.push_back(query.string(0));
        return result;
    }
    if (ids.size() > 500000)
        throw std::runtime_error("Culling selection exceeds the supported catalog size.");
    std::set<std::string> unique;
    for (const auto &id : ids) {
        if (id.empty() || !unique.insert(id).second)
            continue;
        Statement query(db_, "SELECT 1 FROM assets a WHERE a.id=?1 AND NOT EXISTS(SELECT 1 FROM "
                             "culling_analysis c WHERE c.asset_id=a.id AND c.provider=?2 AND "
                             "c.checksum=a.checksum)");
        query.text(1, id);
        query.text(2, provider);
        if (query.row())
            result.push_back(id);
    }
    return result;
}

std::optional<std::string> Catalog::cullingProvider() const {
    Statement query(db_, "SELECT MIN(provider),MAX(provider),COUNT(*) FROM culling_analysis");
    if (!query.row() || query.integer(2) == 0)
        return std::nullopt;
    const auto first = query.string(0), last = query.string(1);
    if (first != last)
        throw std::runtime_error("Catalog contains incompatible local-analysis providers.");
    return first;
}

void Catalog::saveCullingAnalysis(const std::string &id, const std::string &checksum,
                                  const std::string &provider,
                                  const ai::CullingObservation &observation) {
    validateProvider(provider);
    observation.validate();
    if (id.empty() || checksum.empty())
        throw std::runtime_error("Invalid culling asset identity.");
    const auto imageBytes = encodeVector(observation.imageEmbedding);
    execute("BEGIN IMMEDIATE;");
    try {
        Statement incompatible(db_, "SELECT 1 FROM culling_analysis WHERE provider!=?1 LIMIT 1");
        incompatible.text(1, provider);
        if (incompatible.row())
            throw std::runtime_error(
                "Local analysis model changed. Clear analysis before rebuilding it.");
        struct NamedFace {
            std::string person;
            std::vector<float> embedding;
        };
        std::vector<NamedFace> namedFaces;
        Statement previous(
            db_, "SELECT f.person_id,f.embedding,f.dimension FROM face_instances f JOIN people p "
                 "ON p.id=f.person_id WHERE f.asset_id=?1 AND p.provider=?2");
        previous.text(1, id);
        previous.text(2, provider);
        while (previous.row())
            namedFaces.push_back({previous.string(0), decodeVector(previous, 1, 2)});
        Statement write(
            db_,
            "INSERT INTO culling_analysis(asset_id,checksum,provider,sharpness,exposure,"
            "face_quality,eyes_open,score,base_recommendation,recommendation,reason_flags,"
            "dimension,embedding) SELECT id,?2,?3,?4,?5,?6,?7,?8,?9,?9,?10,?11,?12 FROM "
            "assets WHERE id=?1 AND checksum=?2 ON CONFLICT(asset_id) DO UPDATE SET "
            "checksum=excluded.checksum,provider=excluded.provider,sharpness=excluded.sharpness,"
            "exposure=excluded.exposure,face_quality=excluded.face_quality,"
            "eyes_open=excluded.eyes_open,score=excluded.score,"
            "base_recommendation=excluded.base_recommendation,"
            "recommendation=excluded.recommendation,reason_flags=excluded.reason_flags,"
            "dimension=excluded.dimension,embedding=excluded.embedding,updated_at=CURRENT_"
            "TIMESTAMP");
        write.text(1, id);
        write.text(2, checksum);
        write.text(3, provider);
        write.number(4, observation.sharpness);
        write.number(5, observation.exposure);
        write.number(6, observation.faceQuality);
        write.number(7, observation.eyesOpen);
        write.number(8, observation.score);
        write.integer(9, static_cast<int>(observation.recommendation));
        write.integer(10, observation.reasons);
        write.integer(11, static_cast<std::int64_t>(observation.imageEmbedding.size()));
        write.blob(12, imageBytes);
        write.row();
        requireChanged(db_, "Photo changed while creating its culling analysis.");
        Statement clear(db_, "DELETE FROM face_instances WHERE asset_id=?1");
        clear.text(1, id);
        clear.row();
        for (std::size_t index = 0; index < observation.faces.size(); ++index) {
            const auto &face = observation.faces[index];
            const auto faceBytes = encodeVector(face.embedding);
            std::string namedPerson;
            double namedSimilarity = .86;
            for (const auto &candidate : namedFaces) {
                const auto value = similarity(face.embedding, candidate.embedding);
                if (value > namedSimilarity) {
                    namedSimilarity = value;
                    namedPerson = candidate.person;
                }
            }
            Statement insert(
                db_, "INSERT INTO face_instances(id,asset_id,provider,face_index,x,y,width,height,"
                     "quality,eyes_open,dimension,embedding,person_id) VALUES(?1,?2,?3,?4,?5,?6,"
                     "?7,?8,?9,?10,?11,?12,NULLIF(?13,''))");
            insert.text(1, newID());
            insert.text(2, id);
            insert.text(3, provider);
            insert.integer(4, static_cast<std::int64_t>(index));
            insert.number(5, face.bounds[0]);
            insert.number(6, face.bounds[1]);
            insert.number(7, face.bounds[2]);
            insert.number(8, face.bounds[3]);
            insert.number(9, face.quality);
            insert.number(10, face.eyesOpen);
            insert.integer(11, static_cast<std::int64_t>(face.embedding.size()));
            insert.blob(12, faceBytes);
            insert.text(13, namedPerson);
            insert.row();
        }
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

void Catalog::rebuildIntelligentGroups(const std::string &provider, CancellationToken stop) {
    validateProvider(provider);
    struct Candidate {
        std::string id, camera;
        std::int64_t captured = 0;
        double score = 0;
        std::vector<float> embedding;
    };
    struct BurstPlan {
        std::string id;
        std::int64_t started = 0;
        std::vector<std::string> assets;
        std::size_t best = 0;
    };
    struct Cluster {
        std::string id;
        std::vector<float> centroid;
        std::size_t count = 0;
        bool created = false;
    };
    struct Face {
        std::string id;
        std::vector<float> embedding;
    };

    std::vector<Candidate> candidates;
    Statement rows(db_, "SELECT a.id,COALESCE(a.camera,''),a.capture_date,c.score,c.embedding,"
                        "c.dimension FROM culling_analysis c JOIN assets a ON a.id=c.asset_id "
                        "AND a.checksum=c.checksum WHERE c.provider=?1 AND a.capture_date IS "
                        "NOT NULL ORDER BY a.capture_date,a.id");
    rows.text(1, provider);
    while (rows.row()) {
        checkCancellation(stop);
        candidates.push_back({rows.string(0), rows.string(1), rows.integer(2), rows.value(3),
                              decodeVector(rows, 4, 5)});
    }

    std::vector<BurstPlan> bursts;
    const auto planBurst = [&](const std::vector<const Candidate *> &burst) {
        if (burst.size() < 2)
            return;
        const auto best =
            std::max_element(burst.begin(), burst.end(), [](const auto *first, const auto *second) {
                return first->score != second->score ? first->score < second->score
                                                     : first->id > second->id;
            });
        BurstPlan plan{newID(),
                       burst.front()->captured,
                       {},
                       static_cast<std::size_t>(std::distance(burst.begin(), best))};
        plan.assets.reserve(burst.size());
        for (const auto *candidate : burst)
            plan.assets.push_back(candidate->id);
        bursts.push_back(std::move(plan));
    };
    std::vector<const Candidate *> burst;
    for (const auto &candidate : candidates) {
        checkCancellation(stop);
        const bool continues = !burst.empty() && !candidate.camera.empty() &&
                               candidate.captured - burst.back()->captured <= 3 &&
                               candidate.camera == burst.back()->camera &&
                               similarity(candidate.embedding, burst.back()->embedding) >= .82;
        if (!continues) {
            planBurst(burst);
            burst.clear();
        }
        burst.push_back(&candidate);
    }
    planBurst(burst);

    std::vector<Cluster> clusters;
    Statement assigned(db_, "SELECT p.id,f.embedding,f.dimension FROM people p JOIN "
                            "face_instances f ON f.person_id=p.id JOIN culling_analysis c ON "
                            "c.asset_id=f.asset_id JOIN assets a ON a.id=c.asset_id AND "
                            "a.checksum=c.checksum WHERE p.provider=?1 AND f.provider=?1 AND "
                            "c.provider=?1 ORDER BY p.id,f.id");
    assigned.text(1, provider);
    while (assigned.row()) {
        checkCancellation(stop);
        const auto id = assigned.string(0);
        auto vector = decodeVector(assigned, 1, 2);
        auto found = std::find_if(clusters.begin(), clusters.end(),
                                  [&](const auto &cluster) { return cluster.id == id; });
        if (found == clusters.end()) {
            clusters.push_back({id, std::move(vector), 1, false});
        } else if (found->centroid.size() == vector.size()) {
            for (std::size_t i = 0; i < vector.size(); ++i)
                found->centroid[i] = static_cast<float>(
                    (found->centroid[i] * found->count + vector[i]) / (found->count + 1));
            ++found->count;
            found->centroid = ai::normalizedEmbedding(found->centroid);
        }
    }
    std::vector<Face> faces;
    Statement unassigned(db_, "SELECT f.id,f.embedding,f.dimension FROM face_instances f JOIN "
                              "culling_analysis c ON c.asset_id=f.asset_id JOIN assets a ON "
                              "a.id=c.asset_id AND a.checksum=c.checksum WHERE f.provider=?1 AND "
                              "c.provider=?1 AND f.person_id IS NULL ORDER BY f.asset_id,"
                              "f.face_index");
    unassigned.text(1, provider);
    while (unassigned.row()) {
        checkCancellation(stop);
        faces.push_back({unassigned.string(0), decodeVector(unassigned, 1, 2)});
    }
    std::vector<std::pair<std::string, std::string>> assignments;
    assignments.reserve(faces.size());
    for (const auto &face : faces) {
        checkCancellation(stop);
        auto best = clusters.end();
        double bestSimilarity = .90;
        for (auto candidate = clusters.begin(); candidate != clusters.end(); ++candidate) {
            checkCancellation(stop);
            const auto value = similarity(face.embedding, candidate->centroid);
            if (value > bestSimilarity) {
                bestSimilarity = value;
                best = candidate;
            }
        }
        if (best == clusters.end()) {
            clusters.push_back({newID(), face.embedding, 1, true});
            best = std::prev(clusters.end());
        } else {
            if (best->centroid.size() == face.embedding.size())
                for (std::size_t i = 0; i < face.embedding.size(); ++i)
                    best->centroid[i] = static_cast<float>(
                        (best->centroid[i] * best->count + face.embedding[i]) / (best->count + 1));
            ++best->count;
            best->centroid = ai::normalizedEmbedding(best->centroid);
        }
        assignments.emplace_back(face.id, best->id);
    }

    checkCancellation(stop);
    execute("BEGIN IMMEDIATE;");
    try {
        Statement reset(db_, "UPDATE culling_analysis SET recommendation=base_recommendation "
                             "WHERE provider=?1");
        reset.text(1, provider);
        reset.row();
        Statement oldMembers(db_, "DELETE FROM burst_members WHERE asset_id IN(SELECT asset_id "
                                  "FROM culling_analysis WHERE provider=?1)");
        oldMembers.text(1, provider);
        oldMembers.row();
        Statement oldStacks(db_, "DELETE FROM burst_stacks WHERE provider=?1 OR NOT EXISTS(SELECT "
                                 "1 FROM burst_members m WHERE m.stack_id=burst_stacks.id)");
        oldStacks.text(1, provider);
        oldStacks.row();
        for (const auto &plan : bursts) {
            checkCancellation(stop);
            Statement stack(db_,
                            "INSERT INTO burst_stacks(id,provider,started_at) VALUES(?1,?2,?3)");
            stack.text(1, plan.id);
            stack.text(2, provider);
            stack.integer(3, plan.started);
            stack.row();
            for (std::size_t index = 0; index < plan.assets.size(); ++index) {
                checkCancellation(stop);
                Statement member(db_, "INSERT INTO burst_members(stack_id,asset_id,position,"
                                      "is_best) VALUES(?1,?2,?3,?4)");
                member.text(1, plan.id);
                member.text(2, plan.assets[index]);
                member.integer(3, static_cast<std::int64_t>(index));
                member.integer(4, index == plan.best ? 1 : 0);
                member.row();
                Statement recommendation(
                    db_, index == plan.best
                             ? "UPDATE culling_analysis SET recommendation=CASE WHEN "
                               "base_recommendation=-1 THEN -1 ELSE 1 END WHERE asset_id=?1"
                             : "UPDATE culling_analysis SET recommendation=CASE WHEN "
                               "base_recommendation=1 THEN 0 ELSE base_recommendation END WHERE "
                               "asset_id=?1");
                recommendation.text(1, plan.assets[index]);
                recommendation.row();
            }
        }
        for (const auto &cluster : clusters)
            if (cluster.created) {
                checkCancellation(stop);
                Statement person(db_, "INSERT INTO people(id,provider) VALUES(?1,?2)");
                person.text(1, cluster.id);
                person.text(2, provider);
                person.row();
            }
        for (const auto &[face, person] : assignments) {
            checkCancellation(stop);
            Statement assign(db_, "UPDATE face_instances SET person_id=?1 WHERE id=?2 AND "
                                  "provider=?3 AND person_id IS NULL");
            assign.text(1, person);
            assign.text(2, face);
            assign.text(3, provider);
            assign.row();
            requireChanged(db_, "Culling face changed while grouping similar faces.");
        }
        Statement emptyPeople(db_, "DELETE FROM people WHERE provider=?1 AND NOT EXISTS(SELECT 1 "
                                   "FROM face_instances f WHERE f.person_id=people.id)");
        emptyPeople.text(1, provider);
        emptyPeople.row();
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

std::vector<PersonRecord> Catalog::people() const {
    Statement query(
        db_, "SELECT p.id,p.name,COUNT(DISTINCT f.asset_id) FROM people p JOIN "
             "face_instances f ON f.person_id=p.id JOIN culling_analysis c ON "
             "c.asset_id=f.asset_id JOIN assets a ON a.id=c.asset_id AND a.checksum=c.checksum "
             "GROUP BY p.id ORDER BY "
             "CASE WHEN p.name='' THEN 1 ELSE 0 END,p.name COLLATE NOCASE,p.created_at,p.id");
    std::vector<PersonRecord> result;
    while (query.row())
        result.push_back({query.string(0), query.string(1), query.integer(2)});
    return result;
}

void Catalog::renamePerson(const std::string &id, const std::string &name) {
    if (id.empty() || name.size() > 512 || name.find('\0') != std::string::npos ||
        name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Face group name must contain text and fit 128 characters.");
    Statement query(db_, "UPDATE people SET name=?1 WHERE id=?2");
    query.text(1, name);
    query.text(2, id);
    query.row();
    requireChanged(db_, "Face group does not exist.");
}

std::vector<std::string> Catalog::cullingIDs(int recommendation, const std::string &personID,
                                             bool bestOfBurstOnly) const {
    if (recommendation < -2 || recommendation > 1 || personID.size() > 64)
        throw std::runtime_error("Invalid intelligent-library selection.");
    Statement query(
        db_, "SELECT c.asset_id FROM culling_analysis c JOIN assets a ON a.id=c.asset_id AND "
             "a.checksum=c.checksum WHERE (?1=-2 OR c.recommendation=?1) "
             "AND (?2='' OR EXISTS(SELECT 1 FROM face_instances f WHERE f.asset_id=c.asset_id "
             "AND f.person_id=?2)) AND (?3=0 OR EXISTS(SELECT 1 FROM burst_members b WHERE "
             "b.asset_id=c.asset_id AND b.is_best=1)) ORDER BY c.score DESC,c.asset_id");
    query.integer(1, recommendation);
    query.text(2, personID);
    query.integer(3, bestOfBurstOnly ? 1 : 0);
    std::vector<std::string> result;
    while (query.row())
        result.push_back(query.string(0));
    return result;
}

std::int64_t Catalog::applyCullingSuggestions(const std::vector<std::string> &ids) {
    if (ids.empty())
        return 0;
    execute("BEGIN IMMEDIATE;");
    try {
        std::int64_t changed = 0;
        std::set<std::string> unique(ids.begin(), ids.end());
        for (const auto &id : unique) {
            Statement query(db_, "UPDATE assets SET flag=(SELECT recommendation FROM "
                                 "culling_analysis c WHERE c.asset_id=assets.id AND "
                                 "c.checksum=assets.checksum) WHERE id=?1 "
                                 "AND flag=0 AND EXISTS(SELECT 1 FROM culling_analysis c WHERE "
                                 "c.asset_id=assets.id AND c.checksum=assets.checksum)");
            query.text(1, id);
            query.row();
            changed += sqlite3_changes(db_);
        }
        execute("COMMIT;");
        return changed;
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

CullingSummary Catalog::cullingSummary() const {
    CullingSummary result;
    Statement counts(db_, "SELECT COUNT(*),COALESCE(SUM(recommendation=1),0),"
                          "COALESCE(SUM(recommendation=0),0),COALESCE(SUM(recommendation=-1),0) "
                          "FROM culling_analysis c JOIN assets a ON a.id=c.asset_id AND "
                          "a.checksum=c.checksum");
    if (counts.row()) {
        result.analyzed = counts.integer(0);
        result.picks = counts.integer(1);
        result.reviews = counts.integer(2);
        result.rejects = counts.integer(3);
    }
    Statement bursts(db_, "SELECT COUNT(DISTINCT b.stack_id) FROM burst_members b JOIN "
                          "culling_analysis c ON c.asset_id=b.asset_id JOIN assets a ON "
                          "a.id=c.asset_id AND a.checksum=c.checksum");
    if (bursts.row())
        result.bursts = bursts.integer(0);
    Statement peopleCount(db_, "SELECT COUNT(*) FROM people WHERE EXISTS(SELECT 1 FROM "
                               "face_instances f JOIN culling_analysis c ON c.asset_id=f.asset_id "
                               "JOIN assets a ON a.id=c.asset_id AND a.checksum=c.checksum WHERE "
                               "f.person_id=people.id)");
    if (peopleCount.row())
        result.people = peopleCount.integer(0);
    return result;
}

void Catalog::clearCullingIndex() {
    execute("BEGIN IMMEDIATE;");
    try {
        execute("DELETE FROM burst_members;DELETE FROM burst_stacks;DELETE FROM face_instances;"
                "DELETE FROM people;DELETE FROM culling_analysis;");
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
} // namespace luma
