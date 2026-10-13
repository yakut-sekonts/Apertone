# Batch preset / Paste / Sync Settings — 0.4.2 development

Library now applies a saved preset or copied subset to the selected asset IDs.
Sync Settings copies selected Light, Color, Curve and HSL groups from the photo
open in Develop. Selection and settings are captured when the operation starts;
changing a preset later cannot change an in-flight operation. A single transaction
updates the whole selection, with cancellation and visible errors.

Undo batch/Redo batch restore the last successful operation as one transaction.
Newer stored edits cause an all-or-nothing conflict error. No-op and failed
operations retain the preceding journal. The journal is session-local; restart
preserves committed settings but does not preserve batch Undo. The affected open
photo adopts a new history baseline; unaffected photo history is retained.

Preparation waits for active-photo autosave, including retry after a failed save.
Editing, photo switching, export, selection and Library writes are locked while
working. Cancel, close and update installation wait for rollback/completion. A
modal provides progress, retry-save and cancellation. Original Library thumbnails
remain original previews; Develop/export show the applied settings.

Acceptance covers 40 generated catalog assets, independent WAL readers during
writes, selected/excluded groups, missing/duplicate targets, cancellation after
partial work, injected HSL insertion/Undo failures, conflict refusal/retry, no-op
and failed replacement journal retention, active-photo refresh, pending/failed
autosave, QML Sync controls, close during queued/running work, updater readiness,
metadata/collection preservation, restart and unchanged original RAW checksums.
An idle batch does not retain a close lock when autosave rejects closing; Library
close preparation begins only after the editor and batch are ready to close.
Sync accepts long source filenames independently of the preset-name limit.
Existing RAW, color, Library, presets, export/restart and release tests remain
required. No private test asset is published.

Validation: all 15 local CTests passed, including private Nikon acceptance.
clang-format and clang-tidy passed with advisory warnings. macOS Actions must
validate the exact pushed head before merge. Manual native UI inspection and
100k-photo batch performance are not claimed.

See [ADR 0010](adr/0010-batch-settings.md). This does not publish a GitHub release;
the frozen 0.4.0 candidate and its signing requirement remain unchanged.
