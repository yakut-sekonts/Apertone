# Roadmap

This follows the supplied requirements, renamed from Photon to Luma. Each stage
ships an end-to-end workflow with persistent data, error handling and tests.
No dates are promised before measuring scope and performance.

## M0 — RAW Vertical Slice / 0.1 Foundation

[Issue #1](https://github.com/yakut-sekonts/Luma/issues/1) ·
[Implementation PR #2](https://github.com/yakut-sekonts/Luma/pull/2)

✅ Implemented; PR ready for review. Public repository, Apache-2.0, pinned dependencies, macOS CI,
Qt/QML shell, SQLite migration, real Nikon NEF sensor decode, EXIF, CPU Exposure,
autosave and recovery of the selected photo/exposure after restart.

Acceptance: clean clone → build → run → open Nikon D7500 NEF → change Exposure
→ close → reopen → same photo and Exposure. Original checksum must not change.
Public synthetic tests alone do not satisfy the Nikon acceptance criterion.

## 0.2 — Develop Core

✅ Implemented; local acceptance passed. WB controls, exposure/tone, highlight/shadow handling, basic color,
edit schema evolution, undo/redo, disable individual corrections, image export.
Acceptance: edit and export a RAW; restart and reproduce the same render.
Implemented export scope: full-resolution JPEG/PNG/16-bit TIFF, embedded sRGB,
quality and explicit overwrite; no source metadata. History is session-local.
Auto/eyedropper WB, persistent history, advanced export options and DNG remain later work.

## 0.3 — Library

[Issue #5](https://github.com/yakut-sekonts/Luma/issues/5) ·
[Implementation PR #6](https://github.com/yakut-sekonts/Luma/pull/6)

✅ Implemented. Add/Copy import of files/folders, per-file transactional checkpoints,
SHA-256/size duplicate detection, cancellation and dead-owner journal recovery;
lazy original thumbnails, bounded paged metadata, virtualized grid and filmstrip.
Single/multi/range selection, ratings, Pick/Reject/Unflag, color labels, manual
collections and combined basic filters are included. Migration 2→3 preserves edits.
Acceptance covers generated RAWs, GUI organization/restart, Copy integrity and a
100k-row metadata/model fixture. Real 100k-image performance remains unverified.
Move, smart collections, advanced import templates, keywords and XMP remain later work.

## 0.4 — Professional Develop and presets

🚧 In progress. The first substage implements point/parametric Curves, HSL and
targeted tools with atomic persistence, gesture Undo/Redo and reproducible exports.
The user-preset substage implements versioned presets for Light/Color/Curve/HSL,
flat folder organization, create/edit/rename/delete, JSON import/export,
non-destructive hover preview and single-photo subset Copy/Paste. Preset/paste
application is one Undo; edit/catalog schemas remain unchanged.
The batch substage implements multi-photo preset/Paste and subset Sync Settings,
with atomic writes, cancellation, autosave coordination and conflict-aware
session Undo/Redo. The first Detail substage implements Amount/Radius/Detail/
Masking sharpening with transient Option diagnostics, shared preview/export,
neutral catalog migration and preset/batch integration.
The second Detail substage implements manual Luminance Amount/Detail/Contrast
and Color Amount/Detail/Smoothness, independent enable states, Option diagnostics
and noise→sharpening ordering with neutral migration and preset/batch integration.
The Crop substage implements an interactive frame, free/original/fixed/custom
ratios, rotate/straighten/flip and transient composition guides, shared export
geometry, targeted inverse mapping and preset/batch integration.
The Optics substage adds calibrated automatic/manual lens profiles, distortion/
vignetting amounts and lateral CA before Develop/Detail, shared preview/export,
neutral catalog migration and preset/batch integration.
The Transform substage adds manual projective controls, asynchronous Auto with
confidence refusal, Guided vertical/horizontal line pairs and shared inverse
geometry/coverage, with neutral migration and a Geometry settings subset.
Remaining scope: ready-made filters, edited thumbnails and advanced export/queue.
Ready-to-use filters must have representative licensed image tests and no
hidden irreversible effects. Preset application is one Undo operation.

## 0.5 — Local Adjustments

🗓 Planned. Brush, linear/radial gradients, color/luminance ranges, mask boolean
operations, overlay, feathering and per-mask controls.

## 0.6 — Performance

🗓 Planned. Metal processing backend, CPU/GPU tolerance tests, bounded LRU,
progressive quality, true 1:1 rendering, memory pressure handling and latency
benchmarks on 8/16 GB Apple Silicon machines.

## 0.7 — Advanced

🗓 Planned. Optional local AI denoise, AI masks, content-aware removal and
semantic search. Provider interfaces and privacy consent before any cloud option.

## 0.8 — Merge

🗓 Planned. HDR, panorama, alignment, deghosting and editable float outputs.

## 0.9 — Stabilization

🗓 Planned. 10k/100k/500k catalogs, complete metadata/XMP workflow, backups,
missing-file relinking, recovery, fault injection, UI/performance regression tests.

## 1.0 — Stable daily-use release

🗓 Planned. All workflows in requirement §100 pass on supported hardware,
including local masks, presets, batch sync, export and restart. Signing,
notarization, release checksums and known issues accompany the release.
