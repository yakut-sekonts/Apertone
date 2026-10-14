# Perspective Transform — 0.4.7 development

[Issue #24](https://github.com/yakut-sekonts/Luma/issues/24) implements §26.
Develop adds Rotate, Horizontal/Vertical, Aspect, Scale, X/Y Offset and Constrain
Crop. Guided mode accepts two vertical lines, two horizontal lines, or both;
endpoints are transient and only the solved homography is persisted. Auto runs
bounded CPU edge/line detection asynchronously, with cancellation and revision
guards. Uncertain/degenerate scenes are refused without edits or history entries.
Auto and Guided each apply one Undo operation, preserving other settings.

The original projective implementation operates in upright normalized camera
coordinates after Optics/Develop/Detail and before Crop. EXIF, inverse homography
and Crop compose before one bilinear linear-RGB sample. Neutral/disabled Transform
preserves the exact previous pixel path. Constrain Crop calculates an inward zoom
with corner containment and pole checks; the effective scale is visible. Without
it, uncovered pixels are transparent in preview/PNG/TIFF16 and black in JPEG.
Unsafe, singular, non-finite or unbounded matrices are rejected transactionally.
See ADR 0015 for conventions and safeguards.

Auto uses a Gaussian-smoothed luminance preview, Sobel gradients, signed-polarity
Hough voting with an adaptive noise floor, long-line coverage checks, stable line
pairs and the same projective solver as Guided. It has no semantic AI dependency.
The detector deliberately refuses insufficient geometric evidence; it does not
claim reliable correction for every photograph.

Catalog 8→9/edit 7→8 migrates to neutral enabled Transform while preserving prior
settings, metadata, revisions and child tables. Invalid old rows roll back. Old
readers refuse the new catalog. Geometry is the eighth subset in schema-6 presets,
Copy/Paste, atomic batch Sync, Undo/Redo and conflict checks. Older presets retain
absent Geometry. Preview, exports and targeted tools share inverse coordinates.
Guided/Crop/Original/preset/load/export transitions prevent stale canvas editing.

Local validation: all 25 CTests pass, including private Nikon D7500;
clang-format is clean. Validation covers independent homographies and vanishing directions, line
parallelism, EXIF/Crop composition, HDR, exact neutral pixels, transparent encoded
PNG/TIFF/JPEG, poles/singularities, Auto success/refusal/cancellation, actual QML
pointer drags and history, all 255 preset subsets, Geometry batch operations,
neutral byte-identical migration exports, rollback and restart/source integrity.
Actual previous-bundle compatibility and private Nikon full-resolution exports
supplement generated fixtures. Exact-head macOS CI and standalone bundle checks
are required before integration.

This remains a development bundle. The frozen 0.4.0 candidate is unchanged.
Remaining phase 0.4 work includes ready-made filters, edited thumbnails and advanced
export/queue. Local Adjustments remain phase 0.5 and must not start yet.
