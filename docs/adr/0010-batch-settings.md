# ADR 0010: Atomic batch settings and session Undo

Library captures a sorted snapshot of selected asset IDs and a validated settings
snapshot on the GUI thread. Preset, clipboard Paste and Sync from the open source
photo all use the same subset merge for Light/Color/Curve/HSL. Sync identifies its
source in the dialog; the source need not be part of the selection. Unselected
photos, excluded groups, correction states outside the subset, originals, ratings,
flags, collections and metadata are preserved. Missing RAWs can receive parameter
changes without reading or decoding their source files.

One dedicated worker retains a Catalog connection. Batch writes start with
`BEGIN IMMEDIATE`, load the latest target edits, merge selected groups and write
parent/curve/HSL rows through the same internal writer used by single-photo save.
The transaction commits once for the whole selection. Missing/duplicate IDs,
invalid edits, SQL failure and cancellation roll back the entire operation.
SQLite VM progress callbacks make long snapshot/restore SQL cancellable; they are
removed before rollback so cancellation cannot interrupt rollback itself. WAL
readers see the preceding or committed state, never a partial batch.

Before/after documents live in SQLite TEMP tables on this worker connection.
Temporary storage uses FILE with a 4 MiB page-cache target instead of an array of
edit snapshots in application RAM. The selection ID snapshot remains in memory.
Only changed targets enter the Undo journal. The last successful batch can be
undone/redone as one operation. A failed replacement or no-op retains its journal.
Undo/Redo compare stored parameters and child rows against the expected snapshot
under the write lock and refuse conflicts atomically. Revisions/timestamps are
ignored; stored representation, including advanced_saved, is compared strictly.
The journal is session-local and disappears on close; committed edits survive
restart. No new catalog tables or migration are needed (edit schema 3/catalog 4).
100k-photo batch latency and disk usage have not been benchmarked.

The editor ends its gesture, locks editing/loading/export and flushes autosave
before a batch can start. Pending/failed saves keep the batch in preparation;
the modal offers retry or cancel without losing the draft. Library writes and
selection activation are blocked. The worker returns the active photo's edit
snapshot from inside the transaction. After commit it becomes the editor's new
history baseline without an additional autosave; an unaffected active photo
retains its history. Local photo history preceding an affected batch is replaced;
batch Undo restores its values, not the earlier local history stack.

Library's Cmd+Z/toolbar arrows target the batch. Develop first uses its local
history and falls back to the latest batch when local history is unavailable.
Explicit Undo batch/Redo batch controls are also available. A later manual edit
can block batch Undo until its stored values match again. There is no force-undo
that overwrites newer work.

Close and Sparkle preparation cancel pending/running batches and wait for the
worker to finish or roll back. The update readiness guard also includes the batch
and editor save/import/export state. The Catalog connection is destroyed on its
worker before worker shutdown. GUI callbacks carry QPointer lifetime guards.
This substage changes settings only; batch export, edited Library thumbnails and
future Detail/Optics/Geometry/Crop/Masks/Effects groups remain separate work.
