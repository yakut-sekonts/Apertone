# Local Adjustments — 0.5.1 source

Develop → **Local adjustments** opens the local tools. Choose Linear gradient,
Radial gradient, Brush, Luminance Range or Color Range and create a mask. Adjust
local Exposure, Contrast, Highlights, Shadows, Whites, Blacks, Vibrance and
Saturation. A neutral new mask changes no pixels.

Drag on the photo to redraw a gradient or paint a Brush. Linear handles move its
ends/center; radial handles move the center or resize axes, with Rotation for the
ellipse. Brush size is diameter as a percentage of the shorter source dimension;
stroke opacity and erase are captured for each stroke. Feather applies to the
selected primitive, including all its brush strokes. Overlapping segments of one
stroke do not build up opacity; separate strokes do. Continuous capsules connect
pointer samples, including fast movement. Leaving the photo or valid transformed
image coverage ends the stroke; re-entry requires a new press and never paints a
bridge across the excluded region. Mouse input is circular and has no pressure
or Auto Mask support. Clear brush is undoable.

Select a range primitive and use **Sample on photo** to pick its reference. The
worker samples the globally developed/denoised/sharpened image before local
layers. Transparent samples and results made obsolete by another edit, tool or
selection are refused. Luminance selects an interval of clamped encoded linear-Y;
Color selects a radius in clamped encoded sRGB using normalized Euclidean
distance. Feather softens the boundary. These definitions are independent of
display ICC and local layer order; no Adobe matching or perceptual Delta E is
claimed. Negative/HDR reference values clamp to the supported range.

**Combine selections** adds a Linear/Radial/Brush/Luminance/Color operand using
Add, Subtract or Intersect. Select the base or an operand to edit its geometry,
brush or range. Operands can be enabled, inverted, reordered and deleted.
For coverage a and b, Add is a+b−ab, Subtract a(1−b), Intersect ab, in list order.
Mask Invert and Mask opacity apply after composition. Layer enable, editable
name, order and eight Light/Color settings belong to the whole mask.

Masks remain anchored through Crop, EXIF and Transform. Coordinates are normalized
pixel-edge positions in the optically corrected source canvas, before those
operations. Brush radius uses the original source aspect, even for a downsampled
preview. Widen/reset Crop temporarily to edit excluded source regions. Layers
blend their adjustment with the preceding scene-linear result after global
Develop/Detail; local color controls do not recalculate RAW white balance.

**Show mask overlay**, its three colors and Overlay opacity are viewing aids.
The overlay shows the complete selected mask's actual coverage, including
composition, inversion, opacity and source alpha. It never enters export or
Library thumbnails. Entering Crop, Guided Transform, targeted tools, Original,
preset preview, batch editing or export closes on-photo mask editing. Reopen
**Edit mask on photo** to continue. Each slider/pointer gesture, range sample or
component operation is one Undo/Redo operation; history is session-local.

Masks persist with the photo's edits. Preview, advanced export, the immutable
export queue and saved thumbnails use the shared CPU evaluator. Global presets,
XMP presets, Paste and Sync Settings preserve target masks. Transferring masks
through presets/batch is not included in this phase.

## Storage and limits

Catalog schema 11 stores a mandatory version-2 `edit_local_masks` child per photo;
global edit schema remains 8. Migration 9→10 adds neutral gradients; 10→11
validates and upgrades all legacy documents atomically, preserving global
settings/revisions. Corruption rolls the entire migration back. Original files
are never modified. Older Apertone versions refuse catalog 11: back up the catalog
before upgrading if rollback is needed. Automatic catalog backup/recovery remains
phase 0.9.

Limits per photo: 128 masks, 64 operands per mask, 512 total primitives, 256
strokes per primitive, 512 total strokes, 16,384 total points and 1 MiB serialized
mask data. Names allow 80 Unicode code points. Validation rejects unknown
versions, invalid UTF-8, non-finite/out-of-range values, duplicate IDs, empty
strokes and incomplete/trailing data. Global/local settings share a transaction;
missing/corrupt mandatory mask data is reported rather than silently reset.
Batch Undo refuses conflicts with later mask changes.

Preview/detail resolution can affect the underlying image used for range
selection. True 1:1 rendering, Metal and performance benchmarks remain phase 0.6.
Depth Range is conditional on an actual source depth channel; none is currently
ingested, so no Depth selector is exposed. AI masks remain phase 0.7.

See [phase acceptance](PHASE5_ACCEPTANCE.md), ADRs 0022/0024 and Issue #44.
