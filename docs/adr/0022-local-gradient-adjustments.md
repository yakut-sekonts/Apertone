# 0022 — Local gradient adjustments

The human resumed roadmap phase 0.5 after release 0.4.13. Its first delivered
increment is 0.5.0: linear/radial masks with local Light/Color, followed by Brush,
range selection and boolean composition in subsequent phase-0.5 increments.

Mask coordinates are normalized pixel-edge coordinates in the optically corrected
source canvas, before Crop/Transform/EXIF orientation. The UI uses CropGeometry's
inverse mapping at preview resolution. Render evaluates local corrections after
global Develop/Detail and before the existing single geometry sample. Preview,
full-resolution export and thumbnails share the same mask evaluator. Empty,
disabled and neutral local adjustments preserve exact prior pixels.

Each enabled mask sequentially blends its locally adjusted scene-linear color
with the preceding layer. Light/Color is Exposure, Contrast, Highlights, Shadows,
Whites, Blacks, Vibrance and Saturation, using the existing pointwise CPU reference
operators with identity color calibration. These are adjustments of the globally
developed result, not a second RAW decode or a local camera white-balance model.

Catalog 10 adds a mandatory per-asset version-1 local-mask child document. Global
edit schema 8 is unchanged. Strict bounded canonical serialization rejects unknown
versions, non-finite values, duplicate IDs and trailing content. Migration adds
neutral children atomically, and readers refuse missing/corrupt children. Local
and global edits share one SQLite transaction and saved revision. The batch journal
includes masks for conflict detection and atomic Undo; global presets and batch
subsets preserve masks. Mask transfer/presets are not claimed in this increment.

History owns complete edit values; one gesture is one Undo. Selection, tool mode,
overlay color/opacity and ready-render guards are transient. Overlay uses the same
geometrical coverage as rendering and never enters export or saved thumbnails.
Thumbnail fingerprints include canonical masks and a new pipeline identity.

Admission is bounded at 128 masks per photo and 1 MiB serialized data. Invalid
gestures/documents are refused atomically. These are explicit resource limits,
not an unbounded memory claim. Brush/boolean/range and Metal/AI remain later work.
