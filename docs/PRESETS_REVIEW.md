# User presets / next 0.4 substage

The first 0.4 release candidate is frozen at `1240358fd6b228cdc2909b2f0e038ec06bdb02e9`.
This substage targets the subsequent 0.4.1 development version and does not change
the already validated 0.4.0 release notes or candidate artifacts.

## Implemented workflow

In Develop, use + to save the current photo's selected Light, Color, Curve and HSL
groups. Supply an optional folder name to organize presets. Hover temporarily
renders saved settings; click applies them as one Undo. Original comparison,
autosave and export always refer to applied edits.

Select a preset and use the menu to rename, move, change included groups, explicitly
replace settings from the active photo, export JSON or delete. Import validates a
Luma JSON preset and assigns a new local ID. Import/export work without a photo.
Copy selects groups; Paste applies the in-memory snapshot to the current photo
as one Undo, including after switching photos. The system clipboard is untouched.

See [ADR 0009](adr/0009-user-presets.md) for schemas, limits, atomic writes,
corruption handling, alias protection and the distinction between stored and
hovered settings. Unsupported future controls and Adobe preset formats are refused.
There is no catalog migration in this substage.

## Acceptance

The preset acceptance test uses generated RAW data only. It exercises all 15 group
subsets, As Shot preservation, JSON types/ranges/unknown versions, two independent
writers, lock failures, corrupt/duplicate/future collections, rename/move/delete,
export/import, rendered hover pixels, untouched catalog/history/redo, one-operation
Undo, Copy/Paste, export during hover, protected destinations and saved-state reload.
A QML acceptance test opens the create/edit dialogs, saves metadata, selects a
subset and verifies persisted results without binding/runtime warnings.

Local validation passed all 14 CTests, including external Nikon acceptance,
plus clang-format and clang-tidy (existing advisory warnings are not a zero-warning
claim). Generated-image rendering verified that all panel controls fit its default
width. The existing RAW, color, Library, export/restart, release and updater tests
remain required in macOS Actions. Private Nikon acceptance stays local; no private assets enter Git.
Manual UI inspection requires an unlocked Mac and is distinct from automated QML
acceptance. Multi-photo paste/presets, Sync Settings, ready-made filters, Detail,
Crop and Optics are not claimed here.
