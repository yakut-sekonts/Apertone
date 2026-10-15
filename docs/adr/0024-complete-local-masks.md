# 0024 — Brush, range selection and mask composition

Complete phase 0.5 in one feature PR. Keep the mask as an adjustment layer, and
represent its selection as a base primitive plus ordered enabled Add/Subtract/
Intersect operands. Primitive types are Linear, Radial, Brush, Luminance and Color.
An operand may be inverted. Composition uses a+b−ab, a(1−b), ab respectively;
final mask inversion and opacity apply after composition. No cross-mask references
or cyclic expression graph are needed. Existing ordered adjustment layers remain.

Store brush centerlines in normalized source coordinates, with radius relative to
the shorter source dimension and per-stroke opacity/erase. Evaluate continuous
capsules, taking maximum coverage within one stroke, then composite strokes in
order. Event frequency does not change flow. A bounding-volume hierarchy bounds
candidate work without allocating full-resolution coverage planes per component.
Brush feather is a property of the primitive; changing it edits all its strokes.
Pointer interaction uses the existing inverse geometry; projected brush outlines
follow Crop/Transform. Mouse input uses a fixed circular brush; tablet pressure
and Auto Mask are not claimed by this phase. Leaving the photo rectangle or valid
transformed image coverage ends the stroke; re-entry requires a new press and
does not create a bridge across excluded coverage.

Range selectors reference globally developed/denoised/sharpened scene-linear RGB
before any local adjustment layers. Luminance uses encoded sRGB luminance clamped
to [0,1]; Color uses normalized Euclidean distance in clamped encoded sRGB, not
an Adobe algorithm or a perceptual Delta E claim. Reference colors are stored in
that space. Soft boundaries are smoothstep. On-photo sampling runs on a worker
with the same reference passes; transparent samples and obsolete results are
refused. Selection is independent of display ICC, overlay and local layer order.

Mask document version 2 records typed primitives, vector strokes and ordered
operands. Catalog 11 validates and upgrades catalog-10 version-1 documents in one
transaction and updates the neutral-child trigger. Old mask bytes remain readable
by the new parser, while older apps refuse catalog 11. Bound total primitives,
strokes, points and serialized bytes per photo. Global presets/batch subsets still
preserve target masks; mask transfer is not required by this phase's roadmap.

Run numerical and feature acceptance before declaring phase 0.5 complete. Release
publication and a separate regression campaign follow the human's next step.
Depth Range remains conditional: without a real source depth channel no depth
selector is exposed. AI and Metal remain phases 0.7 and 0.6.
