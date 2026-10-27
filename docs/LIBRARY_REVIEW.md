# 0.3 — Library review

Issue: https://github.com/yakut-sekonts/Apertone/issues/5
PR: https://github.com/yakut-sekonts/Apertone/pull/6
Base: `codex/develop-core`; implementation: `codex/library`.

Apertone previously opened one RAW at a time. This stage adds a persistent photo
Library: Add/Copy import, browsing/filtering/selection, metadata organization and
manual collections, with sensor-decoded Develop opening and preserved edits.

Copy journals each intended path/checksum before installing a no-replace verified
file. Each asset/item commit is atomic; cancel preserves completed checkpoints.
Dead-owner recovery atomically claims sessions, registers matching complete
copies, removes only owned temporaries and preserves changed published files.
The chosen duplicate policy survives recovery. Schema 3 retains schema 2 edits.

The real Qt model pages metadata (16×128-row LRU), while grid/filmstrip virtualize
delegates. Selection uses asset IDs; filters precede pagination. Ratings, flags,
labels and membership updates are atomic. Original thumbnails are lazy and
bounded; embedded previews are restricted to Library, never Develop.

Validation: C++ warning-clean build; core and public RAW/GUI restart integration;
Add/Copy/hash integrity, duplicate/per-item failures, cancellation/recovery,
live-owner exclusion, concurrent new-catalog startup, schema 2 migration, bulk
rollback, collections and combined filters. A generated 100k metadata fixture
checks the actual model cache bound and range/select-all. All six CTests pass locally, including private Nikon D7500 regression;
clang-format and clang-tidy pass. An ad-hoc-signed standalone 0.3 bundle passes
native Library/import/collection/Develop acceptance with the Qt SDK unavailable,
plus Nikon Undo/Redo, full-resolution export, restart-identical PNG and unchanged
original checksum. Real 100k-image performance remains unverified.

Scope: macOS 14+ Apple Silicon. No Move, edited thumbnails, smart collections,
advanced import templates/backups, keywords, XMP, persistent Undo or claim of
real 100k-image performance. Professional Develop and presets remain 0.4.

Closes #5. This PR is stacked on #4, which is stacked on #2.
