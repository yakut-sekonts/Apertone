#pragma once
#include <string>

namespace fixture {
inline constexpr const char *removePostV23Tables = R"sql(
    DROP TABLE keyword_aliases;
    DROP TABLE catalog_keywords;
    DROP TABLE lightroom_migration_sets;
    DROP TABLE lightroom_migration_snapshots;
    DROP TABLE collection_set_entries;
    DROP TABLE collection_sets;
    DROP TRIGGER remove_empty_photo_stack;
    DROP TABLE photo_stack_members;
    DROP TABLE photo_stacks;
    DROP TABLE lightroom_smart_rules;
    DROP TABLE lightroom_migration_collections;
    DROP TABLE lightroom_migration_photos;
    DROP TRIGGER validate_virtual_original;
    DROP TRIGGER sync_virtual_filename;
    DROP INDEX assets_virtual_original;
    ALTER TABLE assets DROP COLUMN original_asset_id;
    ALTER TABLE assets DROP COLUMN copy_name;
)sql";
// Used only on generated test catalogs before reconstructing a schema 1/2
// edit_documents table. Remove later features instead of leaving a mixed schema
// that cannot represent an actual historical catalog.
inline const std::string removePostV11Tables = std::string(removePostV23Tables) + R"sql(
    DROP TABLE edit_history_states;
    DROP TABLE burst_members;
    DROP TABLE burst_stacks;
    DROP TABLE face_instances;
    DROP TABLE face_groups;
    DROP TABLE culling_analysis;
    DROP TABLE import_item_plans;
    DROP TABLE import_options;
    DROP TRIGGER smart_collection_rename;
    DROP TABLE smart_collections;
    DROP TRIGGER initialize_creative;
    DROP TABLE edit_creative;
    DROP TABLE edit_versions;
    DROP TABLE camera_profiles;
    DROP TRIGGER initialize_asset_metadata;
    DROP TRIGGER update_organization_metadata;
    DROP TRIGGER update_color_label_metadata;
    DROP TABLE asset_creators;
    DROP TABLE asset_keywords;
    DROP TABLE asset_metadata;
    DROP TABLE derived_images;
    DROP TABLE semantic_embeddings;
    DROP TRIGGER initialize_retouch;
    DROP TABLE edit_retouch;
)sql";
} // namespace fixture
