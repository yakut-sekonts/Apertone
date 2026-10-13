#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
namespace luma {
void Catalog::migrateCrop() {
    {
        detail::Statement unexpected(
            db_, "SELECT 1 FROM edit_documents WHERE schema_version!=5 LIMIT 1");
        if (unexpected.row())
            throw std::runtime_error("Unsupported edit document version during Detail migration.");
    }
    execute(R"sql(
                CREATE TABLE edit_documents_v6(
                    asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
                    schema_version INTEGER NOT NULL DEFAULT 6 CHECK(schema_version=6),
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
                    enabled INTEGER NOT NULL DEFAULT 32767 CHECK(typeof(enabled)='integer' AND enabled BETWEEN 0 AND 32767),
                    curve_shadows REAL NOT NULL DEFAULT 0 CHECK(curve_shadows BETWEEN -100 AND 100),
                    curve_darks REAL NOT NULL DEFAULT 0 CHECK(curve_darks BETWEEN -100 AND 100),
                    curve_lights REAL NOT NULL DEFAULT 0 CHECK(curve_lights BETWEEN -100 AND 100),
                    curve_highlights REAL NOT NULL DEFAULT 0 CHECK(curve_highlights BETWEEN -100 AND 100),
                    advanced_saved INTEGER NOT NULL DEFAULT 0 CHECK(advanced_saved IN (0,1)),
                    sharpen_amount REAL NOT NULL DEFAULT 0 CHECK(sharpen_amount BETWEEN 0 AND 150),
                    sharpen_radius REAL NOT NULL DEFAULT 1 CHECK(sharpen_radius BETWEEN 0.5 AND 3),
                    sharpen_detail REAL NOT NULL DEFAULT 25 CHECK(sharpen_detail BETWEEN 0 AND 100),
                    sharpen_masking REAL NOT NULL DEFAULT 0 CHECK(sharpen_masking BETWEEN 0 AND 100),
                    noise_luminance REAL NOT NULL DEFAULT 0 CHECK(noise_luminance BETWEEN 0 AND 100),
                    noise_luminance_detail REAL NOT NULL DEFAULT 50 CHECK(noise_luminance_detail BETWEEN 0 AND 100),
                    noise_luminance_contrast REAL NOT NULL DEFAULT 0 CHECK(noise_luminance_contrast BETWEEN 0 AND 100),
                    noise_color REAL NOT NULL DEFAULT 0 CHECK(noise_color BETWEEN 0 AND 100),
                    noise_color_detail REAL NOT NULL DEFAULT 50 CHECK(noise_color_detail BETWEEN 0 AND 100),
                    noise_color_smoothness REAL NOT NULL DEFAULT 50 CHECK(noise_color_smoothness BETWEEN 0 AND 100),
                    crop_x REAL NOT NULL DEFAULT 0 CHECK(crop_x>=0),
                    crop_y REAL NOT NULL DEFAULT 0 CHECK(crop_y>=0),
                    crop_width REAL NOT NULL DEFAULT 1 CHECK(crop_width>=0.00005 AND crop_x+crop_width<=1.000000000001),
                    crop_height REAL NOT NULL DEFAULT 1 CHECK(crop_height>=0.00005 AND crop_y+crop_height<=1.000000000001),
                    crop_angle REAL NOT NULL DEFAULT 0 CHECK(crop_angle BETWEEN -45 AND 45),
                    crop_aspect REAL NOT NULL DEFAULT 0 CHECK(crop_aspect IN (-1,0) OR crop_aspect BETWEEN 0.01 AND 100),
                    crop_quarter_turns INTEGER NOT NULL DEFAULT 0 CHECK(typeof(crop_quarter_turns)='integer' AND crop_quarter_turns BETWEEN 0 AND 3),
                    crop_flip_h INTEGER NOT NULL DEFAULT 0 CHECK(crop_flip_h IN (0,1)),
                    crop_flip_v INTEGER NOT NULL DEFAULT 0 CHECK(crop_flip_v IN (0,1)),
                    revision INTEGER NOT NULL DEFAULT 0,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
        INSERT INTO edit_documents_v6(asset_id,exposure,temperature,tint,custom_wb,contrast,highlights,shadows,whites,blacks,vibrance,saturation,enabled,curve_shadows,curve_darks,curve_lights,curve_highlights,advanced_saved,sharpen_amount,sharpen_radius,sharpen_detail,sharpen_masking,noise_luminance,noise_luminance_detail,noise_luminance_contrast,noise_color,noise_color_detail,noise_color_smoothness,revision,updated_at) SELECT asset_id,exposure,temperature,tint,custom_wb,contrast,highlights,shadows,whites,blacks,vibrance,saturation,enabled|16384,curve_shadows,curve_darks,curve_lights,curve_highlights,advanced_saved,sharpen_amount,sharpen_radius,sharpen_detail,sharpen_masking,noise_luminance,noise_luminance_detail,noise_luminance_contrast,noise_color,noise_color_detail,noise_color_smoothness,revision,updated_at FROM edit_documents;
        DROP TABLE edit_documents;
        ALTER TABLE edit_documents_v6 RENAME TO edit_documents;
        INSERT INTO schema_migrations(version) VALUES(7);
        PRAGMA user_version=7;
    )sql");
}
} // namespace luma
