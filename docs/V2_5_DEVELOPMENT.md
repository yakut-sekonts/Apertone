# Apertone 2.1–2.5 development

The 2.1–2.5 work is developed on one branch and is intended for a single
`v2.5.0` release. The 2.1–2.4 labels do not imply published versions. Work is
selected from the existing backlog; the 2.0 release remains the first priority.

## Implemented slices

- User presets support nested folder paths, navigation and atomic folder-tree
  rename/move. Existing folder names remain literal, including `/`. Empty
  folders are not persisted; see [ADR 0009](adr/0009-user-presets.md).
- Camera Raw XMP import accepts Texture, Clarity2012 and Dehaze as sparse
  Presence adjustments. Their rendering is not claimed to match Adobe; the
  existing shared Presence enable switch applies to the whole group. See
  [XMP compatibility](XMP_PRESETS.md).
- Export naming can use captured camera, lens, ISO and UTC time from the
  immutable export snapshot. Missing metadata uses explicit fallback values;
  see [export](EXPORT.md).
- Batch Apply/Paste/Sync and batch Undo/Redo retain each changed photo's history
  in the same transaction as its edits. The session batch journal is not
  persistent across restart; see [ADR 0010](adr/0010-batch-settings.md).
- Library can review matching stored SHA-256 and byte size across the catalog.
  These are import-time fingerprints, not proof of current file identity: the
  originals may have changed or become unavailable. The review does not open
  or modify originals. Pages contain at most 40 assets, and a large group can
  span pages. Concurrent catalog changes may alter results between pages;
  Refresh starts a new traversal. Local Library reload invalidates the view.
  Back navigation retains up to 128 page positions.

## Verification still required

Each change is checked with its direct dependencies. Before the single 2.5
release, run the integrated gate appropriate to the final scope and verify the
release artifacts and updater. Batch history has targeted transaction, restart,
conflict and UI coverage. An opt-in scale test exercises the actual 256 MiB
history limit using valid bitmap snapshots and checks that direct save and
batch Apply reject excess data without changing saved edits, history or the
batch journal. UX-BAT-014 has not passed for this change: the native test
window could not become active while the Mac was locked.

The fingerprint review has targeted read-only, query-plan, pagination,
asynchronous model and offscreen dialog coverage. Cancellation before a query and SQLite progress-handler interruption after the
query starts are tested. The in-flight test also rejects a build with the
progress handler disabled and verifies that a subsequent read still succeeds. The dialog has not been tested in a
native macOS window.

The standalone release verifier now checks catalog schema 23 and `edited-v6`
thumbnail identities, matching the application and integration tests. The
complete verifier passed against the published v2.0.0 application using
temporary catalogs and generated fixtures. The historical v2.0.0 recovery
continues to verify its original tagged source and recorded test corrections.

The fingerprint dialog's isolated QML test also passed using the Cocoa platform
and Metal backend; it uses a mock Library model and reported that the window
was not active, so it does not replace a native application workflow check.
The UX test launcher now accepts a symlinked build-root path while still
rejecting executables outside that root; configure and its CTest pass in `/tmp`.
