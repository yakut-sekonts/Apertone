# ADR 0011: Shared spatial sharpening and transient diagnostics

## Status

Accepted for the first Detail substage (0.4.3 development).

## Context

The supplied requirements §23 specify Amount, Radius, Detail, Masking and
Option diagnostics. DevelopPipeline currently processes independent pixels;
sharpening needs neighboring developed luminance. Preview is area-reduced camera
RGB, while PNG/JPEG and TIFF16 export process full-resolution decoded data.

## Decision

Add a shared SharpeningPass after DevelopPipeline and before ICC/quantization.
It computes linear-sRGB luminance and a separable Gaussian blur with replicated
edges. Two Float32 luminance planes and a bounded horizontal-row ring avoid a
second full RGB image. Amount scales the unsharp residual; Detail controls a
relative soft threshold that suppresses low-amplitude texture/noise; Masking
weights the result using relative gradients of blurred luminance. An equal delta
added to all RGB channels preserves channel differences. Signed/HDR values are
not clipped in this pass. This is Luma's own CPU algorithm, not Adobe's algorithm.

Radius (0.5–3) is measured in original-image pixels. Preview records its reduction
scale and applies it to the Gaussian radius. Amount defaults to 0, so migrated
photos and existing neutral renders remain unchanged. Cancellation is checked
per scanline. Both export paths use this pass without diagnostic state.

Option-drag diagnostics are editor-only state: sharpened luminance for Amount,
centered residuals for Radius/Detail and an edge mask for Masking. Gesture end,
modifier release, app deactivation and photo/edit transitions clear diagnostics.
History/autosave/presets/export store only actual parameters and enabled state.

Catalog 4→5 rebuilds only the edit parent table transactionally, preserving its
columns, revision/timestamp and Curve/HSL children (which reference assets).
New parameters and the sharpening correction flag form edit schema 4. Failure
rolls back the entire migration; future catalogs are refused. Legacy apps refuse
the new catalog schema rather than silently dropping settings.

Detail is a fifth settings group. Preset schema 2 writes its sharpening fields;
schema 1 remains readable and cannot contain Detail. Both preset collection
versions are accepted, unknown versions remain protected. Excluded groups retain
their parameters and enabled states. Batch journals include the new columns.

## Consequences

Spatial processing adds two luminance planes and a second point-processing pass
when active. Disabled/zero Amount takes the prior rendering path. Fit previews
cannot replace 1:1 inspection; true 1:1 remains in the Performance stage. Noise
reduction and export-specific output sharpening remain separate work. No claim
of equivalence to Lightroom, camera-profile quality or 100 MP performance is made.
