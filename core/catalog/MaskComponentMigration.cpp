#include "core/catalog/Catalog.h"
#include "core/catalog/LocalMaskStorage.h"
#include "core/catalog/SqliteStatement.h"
namespace luma {
void Catalog::migrateMaskComponents() {
    detail::Statement parents(
        db_, "SELECT asset_id,schema_version FROM edit_documents ORDER BY asset_id");
    while (parents.row()) {
        if (parents.integer(1) != 8)
            throw std::runtime_error("Unsupported global edit version during mask migration.");
        const auto id = parents.string(0);
        detail::Statement document(db_, "SELECT document FROM edit_local_masks WHERE asset_id=?1");
        document.text(1, id);
        if (!document.row() || !document.string(0).starts_with("LUMA_LOCAL_MASKS 1 "))
            throw std::runtime_error("Missing or unsupported legacy mask document.");
        writeLocalMasks(id, detail::readLocalMasks(db_, id));
    }
    execute(R"sql(
        DROP TRIGGER initialize_local_masks;
        CREATE TRIGGER initialize_local_masks AFTER INSERT ON edit_documents BEGIN
            INSERT INTO edit_local_masks VALUES(NEW.asset_id,'LUMA_LOCAL_MASKS 2 0'||char(10));
        END;
        INSERT INTO schema_migrations(version) VALUES(11);
        PRAGMA user_version=11;
    )sql");
}
} // namespace luma
