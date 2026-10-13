# ADR 0013: Crop geometry shared by display, export and targeting

## Status

Accepted for the Crop substage (0.4.5 development).

## Decision

Implement requirements §27: normalized crop bounds in the upright canvas,
free/original/fixed/custom aspect, quarter-turn rotation, ±45 degree straighten,
horizontal/vertical flips, and transient thirds/grid/diagonal/golden-ratio guides.
Perspective, guided Transform and lens corrections remain separate stages.

CropSettings is typed and versioned: x/y/width/height, angle, integer quarterTurns,
boolean flips and aspect (0 free, -1 original, positive locked ratio). Invalid,
non-finite, out-of-bounds and fractional integer/boolean data is rejected atomically.
A new Crop settings subset includes the enable flag. Stored aspect is applied at
render time by centering the largest matching rectangle inside the proposed bounds,
so copying to another camera/rotation retains the intended physical ratio.

Shared CropGeometry follows pointwise Develop, noise reduction and sharpening,
then precedes ICC/quantization. Inverse mapping combines metadata orientation,
quarter turns, flips and straighten. An inscribed canvas excludes empty rotated
corners, using uniform scale bounds and original pixel spacing. Cropping uses
rounded pixel edges with a minimum one-pixel output. Fractional rotation uses
bilinear sampling of processed linear RGB; neutral/quarter-turn/flip/crop paths
sample exact pixels. No full transformed Float32 image is allocated. Preview and
TIFF16 use the same mapping; PNG/JPEG retain the shared display rendering path.
Targeted adjustments inverse-map the displayed crop to the source preview.

Crop editing is transient UI state. It displays the safe full transformed canvas
with a movable/resizable frame; only CropSettings enter autosave/history/export.
The frame activates only when the matching uncropped canvas render is ready.
Bounds-only dragging does not rerender unchanged full-canvas pixels; one drag is
one Undo. Photo changes, Original, preset preview, export and external edits leave
the tool. Guides are presentation state and never reach output pixels.

Catalog 6→7 transactionally rebuilds the parent as edit schema 6, initializes
neutral crop, preserves all older data/children/selection/revisions and rolls back
on invalid legacy rows. Older builds refuse schema 7. Preset schema 4 reads 1/2/3;
Crop is only legal in schema 4 and missing Crop groups preserve current geometry.
Batch snapshots/conflicts/Undo include all geometry columns. Crop defaults to
neutral and preserves all existing rendered pixels.

## Consequences

Bilinear resampling can soften a straightened image; higher quality resampling is
future work. Preview rounding differs slightly from full-resolution pixel edges.
Normalized presets preserve composition proportionally, not subject-aware framing.
No auto perspective, guided lines, optics, Metal or public release is included.
