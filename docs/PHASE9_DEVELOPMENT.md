# Phase 0.9 source development

Status: **initial implementation, unbuilt and unqualified**. Catalog schema is
16. Source versions are development markers, not published releases. Stable
remains 0.5.1; its catalog must be restored from a pre-upgrade snapshot to downgrade.

## Metadata and XMP

Import reads descriptive EXIF/IPTC/XMP through ImageIO alongside the existing
camera/exposure reader. Existing sidecars override the matching embedded fields.
Unsupported metadata does not prevent pixel import: the catalog records a warning
and the Metadata dialog displays it. Existing catalog records are not silently
replaced on reopening a photo.

Library → Photo actions → Photo metadata edits Title, Caption, Copyright,
Creator, Location, Keywords, Rating and Label. Database revisions reject stale
saves. Keywords are ordered, unique, bounded and searchable; Title/Caption are
also included in Library search. Unicode byte/control limits are enforced in
the core. The first metadata draft of an old record reads the source, but changes
to the catalog require Save or the explicitly confirmed Read original / XMP.

Saved export snapshots include descriptive metadata and organization. All,
Copyright only, Remove location and None policies filter the merged source and
catalog metadata before serialization. A never-initialized record only overrides
rating/label, retaining other embedded descriptive fields.

RAW sidecars use `basename.xmp`; raster sidecars use `filename.ext.xmp` to avoid
RAW/JPEG name collisions. Ambiguous RAW basenames or distinct .xmp/.XMP files
are refused. XMP is UTF-8, bounded to 8 MiB, with no DTD/entities or symbolic links.
ImageIO rewrites descriptive properties while retaining other parsed properties
and localized text variants. It does not promise byte-identical serialization
of unrelated XML. Every existing packet is backed up verbatim by SHA-256 in
`<catalog>.luma-xmp-backups` before replacement.

Sidecar writes use same-folder temporary files, fsync, revision/hash conflict
checks, no-replace installation for new files and atomic rename for known files.
External changes cause an error; catalog edits remain saved. Pending revisions
survive restart and can be retried manually or with opt-in automatic metadata
writing. There is an unavoidable external-writer race between the final filesystem
comparison and rename; the old packet is retained in the backup. This is not an
interprocess transaction with third-party editors. Develop settings are not
exported as Adobe Camera Raw adjustments.

Current import limits: a single Creator, standard five-color labels and ratings
0–5. Multiple creators, custom labels or other unsupported representations produce
a visible warning rather than silently flattening data. These interoperability
cases require a product decision/fixtures during final qualification.

## Backup and recovery

Settings → Catalog and backups provides Every launch, Daily, Weekly or Manual
snapshots, 1–50 retained automatic snapshots (default 10), backup-folder access
and restore into a **new** file. Daily/weekly intervals are evaluated at startup.
SQLite's online backup includes committed WAL state. A new snapshot must pass
SQLite integrity and foreign-key checks before pruning; migration snapshots are
never included in automatic pruning. Corrupt old snapshots are preserved and
reported. These checks are application implementation, not executed QA evidence.

Restoration validates the source read-only and refuses an existing destination.
The active catalog is not replaced. The restored file can be opened in a new
window from this dialog. Catalog backup contains the SQLite data, not original
photos, generated DNG files, presets, preference files or user-selected models;
the UI explicitly asks for separate photo backups.

Create/Open catalog starts another desktop process with an explicit catalog path.
A per-catalog desktop lock prevents simultaneous desktop owners. CLI/third-party
SQLite connections still rely on SQLite concurrency and must not be treated as
desktop-lock participants. Settings/presets remain shared by catalogs in the same
directory, as in previous releases; separate project folders isolate them.

## Missing files and external changes

Loaded Library/filmstrip pages show Missing from worker-side filesystem checks.
Locate verifies size and SHA-256 before changing a missing asset's path. Locate
Folder maps old relative paths into the selected new root; the optional nearby
fallback checks the same basename immediately under that root. Updates preserve
asset IDs, edits, masks, retouch, organization and collections. Folder updates
checkpoint each metadata page; cancellation retains completed pages.

The watcher observes at most 512 directories associated with loaded metadata
pages, debounces notifications and refreshes their state. It does not poll/hash
all 500k originals, automatically follow a rename, replace changed bytes or
automatically import external XMP. Filesystem changes outside watched pages are
detected when those pages are loaded. Disconnected-volume and filesystem event
behavior remain unqualified; no whole-catalog live-watching claim is made.

## Outstanding qualification and source decisions

- Build every target and update fixtures for schema 16 and the Missing role.
- Exercise multilingual/large metadata, external XMP round trips, unsupported
  namespaces, concurrency, permission/disk failures and pending-write recovery.
- Cover backup retention, migration/restore, derived journals and original
  invariants under fault injection; originals/DNGs need independent backup.
- Cover moved roots, duplicates, disconnected volumes, checksums, selected-photo
  reopening and preservation of viewport/selection. LUMA-BUG-035 remains open.
- Run 10k/100k/500k catalog and 8/16 GB resource tests only during final QA.
- Select/qualify distributable denoise and Sky models and their delivery policy.
- Full final functional, image-quality, native UX, recovery, load and stress
  qualification is pending. None of these checks is marked PASS.

Platform references: [ImageIO metadata](https://developer.apple.com/documentation/imageio/cgimagemetadata),
[XMP string access](https://developer.apple.com/documentation/imageio/cgimagemetadatacopystringvaluewithpath(_:_:_:)).
