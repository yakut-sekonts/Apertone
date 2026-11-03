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

## Verification still required

Each change is checked with its direct dependencies. Before the single 2.5
release, run the integrated gate appropriate to the final scope and verify the
release artifacts and updater. Batch history has targeted transaction, restart,
conflict and UI coverage; the actual 256 MiB history limit and the separate
UX-BAT-014 load scenario have not been exercised for this change. Do not mark
them as passed without evidence.
