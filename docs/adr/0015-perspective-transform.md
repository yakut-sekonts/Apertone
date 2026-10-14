# 0015 — Manual and guided projective geometry

Status: accepted for phase 0.4, requirements §26, Issue #24.

Add a typed TransformSettings document: Rotate (±45°), Horizontal/Vertical
(±100), Aspect (±100), Scale (50–200%), X/Y Offset (±100), Constrain Crop and a
solved normalized guide homography. Manual controls compose after the guide
matrix. The transform operates in upright camera coordinates after Optics,
Develop and Detail and before Crop. EXIF orientation and Crop's inverse map are
composed with the inverse homography before one bilinear sample of processed
linear RGB. Neutral/disabled geometry preserves the existing exact pixel path.

Matrices and parameters are finite, bounded and non-singular; an inverse pole
inside the rendered domain is rejected. Constrain Crop finds the smallest inward
scale whose inverse-mapped corners lie within the source rectangle; convexity of
a pole-free projective map makes this a complete containment check. The effective
scale is visible. Without Constrain Crop, source-exterior samples are transparent
in preview/PNG/TIFF, and black in opaque JPEG. This never fabricates edge pixels.

Guided mode displays the full upright source canvas, with current Optics/Develop/
Detail and transient draggable line endpoints. It is mutually exclusive with Crop
and targeted tools and only becomes interactive after the corresponding render
arrives. Two vertical, two horizontal or both line pairs determine vanishing
directions, projective rectification and affine alignment. Degenerate/short/unsafe
guides are refused without changing edits. Apply replaces Transform as one Undo,
preserving Crop and other groups; only the solved matrix is persisted.

Auto uses bounded CPU edge/line detection on an upright preview with Transform
and Crop excluded. It operates asynchronously with cancellation/revision guards,
requires independently supported long line pairs and refuses uncertain or unsafe
solutions. It never silently guesses a correction on scenes without usable lines.
An accepted Auto result replaces Transform in one Undo, preserving other groups.

Catalog 8→9/edit 7→8 migration initializes neutral enabled Transform and preserves
all previous state transactionally. Geometry is a separate subset in presets,
Copy/Paste, atomic batch Sync and Undo/conflict checks. Preset schema 6 reads 1–5
without introducing absent Geometry. Guides/tool mode are transient UI state;
Original, presets, photo changes, export and batch preparation close the tool.

Verification includes independent homographies/vanishing directions, parallelism,
EXIF/Crop composition, safe/transparent bounds, signed HDR/identity, degeneration,
Auto confidence/cancellation, QML pointers/history/transitions, migration/rollback,
restart-identical exports and standalone bundle. Masks remain phase 0.5.
