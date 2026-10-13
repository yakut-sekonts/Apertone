#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
namespace luma {
void Catalog::migrateSharpening() {
    {
        detail::Statement unexpected(
            db_, "SELECT 1 FROM edit_documents WHERE schema_version!=3 LIMIT 1");
        if (unexpected.row())
            throw std::runtime_error("Unsupported edit document version during Detail migration.");
    }
    execute(R"sql(
                CREATE TABLE edit_documents_v4(
                    asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
                    schema_version INTEGER NOT NULL DEFAULT 4 CHECK(schema_version=4),
                    exposure REAL NOT NULL DEFAULT 0 CHECK(exposure BETWEEN -5 AND 5),
                    temperature REAL NOT NULL DEFAULT 6500 CHECK(temperature BETWEEN 2000 AND 25000),
                    tint REAL NOT NULL DEFAULT 0 CHECK(tint BETWEEN -150 AND 150),
                    custom_wb INTEGER NOT NULL DEFAULT 0 CHECK(custom_wb IN (0,1)),
                    contrast REAL NOT NULL DEFAULT 0 CHECK(contrast BETWEEN -100 AND 100),
                    highlights REAL NOT NULL DEFAULT 0 CHECK(highlights BETWEEN -100 AND 100),
                    shadows REAL NOT NULL DEFAULT 0 CHECK(shadows BETWEEN -100 AND 100),
                    whites REAL NOT NULL DEFAULT 0 CHECK(whites BETWEEN -100 AND 100),
                    blacks REAL NOT NULL DEFAULT 0 CHECK(blacks BETWEEN -100 AND 100),
                    vibrance REAL NOT NULL DEFAULT 0 CHECK(vibrance BETWEEN -100 AND 100),
                    saturation REAL NOT NULL DEFAULT 0 CHECK(saturation BETWEEN -100 AND 100),
                    enabled INTEGER NOT NULL DEFAULT 4095 CHECK(typeof(enabled)='integer' AND enabled BETWEEN 0 AND 4095),
                    curve_shadows REAL NOT NULL DEFAULT 0 CHECK(curve_shadows BETWEEN -100 AND 100),
                    curve_darks REAL NOT NULL DEFAULT 0 CHECK(curve_darks BETWEEN -100 AND 100),
                    curve_lights REAL NOT NULL DEFAULT 0 CHECK(curve_lights BETWEEN -100 AND 100),
                    curve_highlights REAL NOT NULL DEFAULT 0 CHECK(curve_highlights BETWEEN -100 AND 100),
                    advanced_saved INTEGER NOT NULL DEFAULT 0 CHECK(advanced_saved IN (0,1)),
                    sharpen_amount REAL NOT NULL DEFAULT 0 CHECK(sharpen_amount BETWEEN 0 AND 150),
                    sharpen_radius REAL NOT NULL DEFAULT 1 CHECK(sharpen_radius BETWEEN 0.5 AND 3),
                    sharpen_detail REAL NOT NULL DEFAULT 25 CHECK(sharpen_detail BETWEEN 0 AND 100),
                    sharpen_masking REAL NOT NULL DEFAULT 0 CHECK(sharpen_masking BETWEEN 0 AND 100),
                    revision INTEGER NOT NULL DEFAULT 0,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
        INSERT INTO edit_documents_v4(asset_id,exposure,temperature,tint,custom_wb,contrast,highlights,shadows,whites,blacks,vibrance,saturation,enabled,curve_shadows,curve_darks,curve_lights,curve_highlights,advanced_saved,revision,updated_at) SELECT asset_id,exposure,temperature,tint,custom_wb,contrast,highlights,shadows,whites,blacks,vibrance,saturation,enabled|2048,curve_shadows,curve_darks,curve_lights,curve_highlights,advanced_saved,revision,updated_at FROM edit_documents;
        DROP TABLE edit_documents;
        ALTER TABLE edit_documents_v4 RENAME TO edit_documents;
        INSERT INTO schema_migrations(version) VALUES(5);
        PRAGMA user_version=5;
    )sql");
}
} // namespace luma
