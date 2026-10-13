# ADR 0009: Versioned user presets and temporary preview

Presets contain an explicit subset of currently implemented settings groups:
Light, Color (including white-balance mode), Curve and HSL. Merging operates on
validated `EditDocument` values in the Qt-free core. Only selected values and
correction flags change. As Shot remains As Shot. Applying a preset or pasting
settings ends an existing gesture and adds one whole-document history operation;
a no-op adds none. The edit and catalog schemas remain 3 and 4.

A schema-1 `luma.develop-preset` JSON document stores name, folder and only the
selected groups. Required fields, JSON types, finite numbers, parameter ranges,
curve structure and group flags are validated. Unknown fields/groups and future
schemas are rejected rather than defaulted. Export carries no asset paths, UUIDs,
RAW data, metadata, catalog location or absolute white-balance gains. It is not an
Adobe preset format. Camera-relative As Shot can look different on another photo.

`PresetStore` uses Qt JSON and filesystem support in `apps/shared`, keeping Qt out
of the core. A `luma.preset-library` schema-1 collection lives in `presets.json`
beside the catalog. Catalogs in the same directory share it; selecting an isolated
catalog directory also isolates presets. Local UUIDs identify entries independently
of duplicate names. Folder names are flat labels attached to presets; empty and
nested folder management is not implemented. Renaming or moving preserves the
saved settings. Removing groups is allowed without recapturing; adding previously
absent groups requires explicit recapture from the active photo.

Collections are limited to 512 presets and 1 MiB, whichever limit is reached first.
Names/folders are trimmed, limited to 80 UTF-16 code units and reject control
characters. Small bounded collection operations run synchronously in the desktop
controller. A non-blocking `QLockFile` coordinates writers; each mutation reloads
under the lock and `QSaveFile` atomically replaces the collection. A corrupt,
unsupported, locked or unwritable collection remains unchanged and produces an
error. Other entries from another writer are retained; simultaneous modifications
of the same entry use last successful write. Explicit refresh reloads external
changes. Preset export cannot replace the active RAW, catalog/WAL/SHM or preset
collection, including existing filesystem aliases.

Hover merges into a separate optional preview document. The renderer reads this
snapshot while editor controls, history, autosave and export retain the applied
document. Cancelled renders and revision guards discard obsolete hover pixels.
Leaving hover, opening another photo, editing, Original comparison, export or
preparing an update clears it. Copy/Paste uses an in-memory subset snapshot in
this application session; it does not access the system clipboard. Multi-photo
paste, batch preset application, Sync Settings, ready-made filters and later
Detail/Optics/Geometry/Crop/Masks/Effects groups remain separate stages.
