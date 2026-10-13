# Crop and Straighten — 0.4.5 development

[Issue #20](https://github.com/yakut-sekonts/Luma/issues/20) implements requirements
§27. Crop has free/original/fixed/custom locked ratios, aspect orientation swap,
quarter turns, ±45° straighten, horizontal/vertical flips, movable frame and eight
resize grips. Thirds/grid/diagonal/golden-ratio guides are display-only state.

CropGeometry inverse-maps upright output through metadata orientation, quarter
rotation, flips and straighten into source coordinates. Spatial processing remains
before geometry; bilinear interpolation is in processed linear RGB before ICC or
TIFF16 quantization. Neutral/right-angle/flip/crop cases sample exact pixels.
An inscribed same-aspect canvas excludes empty corners; full canvas dimensions
shrink instead of upscaling. Locked aspect centers the largest matching rectangle
inside proposed normalized bounds. The frame is in the final transformed canvas;
changing transforms reapplies its normalized bounds/locked ratio. Presets transfer
proportional framing to another image. Pixel edges round with minimum 1-pixel output.

The transient tool shows the full transformed canvas; only typed CropSettings and
the enable flag persist. The frame activates only after the matching uncropped render arrives and is
suspended during geometry rerenders. Bounds dragging updates the overlay without rerendering
unchanged image pixels and creates one Undo. Targeted adjustments inverse-map the
displayed crop. Original, preset preview, photo changes, batch preparation and
export close the tool. Export uses actual saved crop and excludes guides.

Catalog 6→7/edit 5→6 preserve prior Detail/Curves/HSL, flags, selection, metadata,
revisions/timestamps and children while initializing neutral geometry. Invalid
legacy rows abort the transaction. Older builds refuse the upgraded catalog.
Stored integer rotations/boolean flips reject fractional/corrupt values without
truncation. Compound crop parameters validate together in CLI/presets, independent
of JSON/argument order, with no partial update. Preset schema 4 reads 1/2/3;
missing Crop groups remain absent. The sixth Crop subset supports presets,
Copy/Paste, Sync and conflict-aware atomic batch Undo/Redo.

Acceptance includes exact pixels for 128 metadata/quarter/flip combinations, crop
pixel edges, ratios and bounds, 40 straighten/orientation cases without empty
corners, bilinear signed/HDR affine/constant sampling, one-pixel crops, numeric/
compound validation and cancellation. Integration covers schema preservation/
rollback, malformed stored types, neutral pixels, full-size PNG/JPEG/TIFF16 and
restart/ICC, all 63 subsets and legacy 1/2/3 presets, QML MouseArea pointer events,
all grips/move, ratios/custom validation, rotate/flip/angle, guide isolation, tool
transitions, targeted availability and batch subset/Sync/Undo. Additional local
checks use a real Nikon D7500 TIFF16 and the actual 0.4.4 application for neutral
migration and old-reader refusal. No private photo/path is published. Exact-head
macOS CI must pass before merge. All 21 local CTests passed on the final geometry;
clang-format/full clang-tidy and rechecks of the final changed sources passed.

No manual native UI inspection, maximal-area straighten crop, higher-quality
resampling, perspective/auto/guided Transform, lens profiles or Metal is claimed.
See [ADR 0013](adr/0013-crop-geometry.md). Frozen 0.4.0 artifacts/signing are unchanged.

CI exposed a race in the existing immediate-close acceptance on tiny exports:
I/O could finish before the caller requested Close. Export admission now runs on
the next UI event-loop turn while exporting remains true, so immediate cancel/
close signals precede worker start. Captured settings and cancellation token remain
unchanged; normal exports retain the existing atomic installation behavior.
