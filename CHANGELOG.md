# Changelog

## 0.4.12 / Source extension before Local Adjustments

- Ordinary image Open/Add/Copy alongside RAW: JPEG, PNG, TIFF8/16, HEIC/HEIF,
  WebP, BMP, GIF and native-codec AVIF.
- ICC-aware full-image decode, EXIF orientation, metadata and alpha through Develop/export.
- Shared Library, thumbnails, presets, batch/queue and durable restart; immutable originals.

## 0.4.11 / Source extension before Local Adjustments

- English default with runtime Russian, Simplified Chinese, Armenian, German,
  Spanish, Italian and French language packages.
- Atomic persisted preference and full retranslation without changing user data.

These are source/development versions; the public release remains v0.3.0.

## 0.3.0 — 2026-10-12 / First macOS release and Library

- Standalone macOS Apple Silicon DMG/ZIP, checksums, build provenance and release notes.
- Verified tag-driven release workflow with repository-scoped publishing permissions.
- Catalog schema 3: manual collections, membership indexes and per-file import journal.
- Add/Copy import, recursive folder scan, SHA-256/size duplicate detection and cancellation.
- No-replace Copy installation, complete-item checkpoints and dead-owner recovery.
- Bounded paged metadata model; virtualized grid and Develop filmstrip.
- Lazy original thumbnail cache with embedded-preview/RAW fallback.
- Single/multi/range/select-all; rating, flags, labels, collections and combined filters.
- Concurrent catalog initialization, rollback/recovery and 100k metadata fixture coverage.

Known limitations: original thumbnails; no Move, smart collections, import naming/
backup templates, keywords, XMP or real 100k-image performance certification.

## 0.2.0 / Develop Core

- Version 2 edit documents and transactional M0 catalog migration.
- Manual Temperature/Tint, As Shot and named WB presets; camera-space preview.
- CPU Exposure/tone/Vibrance/Saturation with per-correction enable switches.
- Gesture-based session Undo/Redo, reset and Cmd+Z/Cmd+Shift+Z.
- Full-resolution JPEG/PNG/16-bit TIFF export with deterministic embedded sRGB ICC.
- Atomic destination install, overwrite policy, RAW/catalog protection and cancellation.
- CLI Develop/export commands and restart-identical GUI export acceptance.

Known limitations: reference WB/tone processing; no Auto/eyedropper WB, persistent
history, camera profile calibration, advanced output settings or export queue.

## 0.1.0 / M0

- Initial C++20 / Qt 6 / QML macOS Apple Silicon application.
- Real integer Bayer RAW decode and independent Float32 CPU pipeline.
- Exposure, As Shot white balance, camera color matrix and ColorSync/LittleCMS display output.
- SQLite catalog, durable autosave, selected-photo and exposure restoration.
- CLI, generated public DNG tests, optional Nikon D7500 acceptance and macOS CI.

Known limitations: this milestone is not the first daily-use version. Bayer only;
bilinear demosaic; Fit viewer; one current photo; no export, manual WB, undo, masks,
preset filters, library grid or Metal pixel processing. Missing and modified
originals retain their edits but need manual source restoration.
