# Detail sharpening — 0.4.3 development

[Issue #16](https://github.com/yakut-sekonts/Apertone/issues/16) implements the first
Detail substage from requirements §23. Develop exposes Amount, Radius, Detail
and Masking, a single enabled-state switch, gesture Undo/Redo and autosave.

SharpeningPass applies a luminance unsharp residual after tone/color processing,
using separable Gaussian blur and two Float32 planes plus a bounded row ring.
Detail soft-thresholds low-amplitude residuals; Masking uses a relative gradient
mask. RGB channel differences, signed values and HDR are retained before output
conversion. Amount=0/disabled follow the preceding path without spatial buffers.
Radius is specified in original-image pixels and scales with the reduced preview.
PNG/JPEG and TIFF16 share the same pass at full resolution. Cancellation stops
inside the luminance and Gaussian phases; no partial exported file is installed.

Option-drag diagnostics show sharpened luminance, centered radius/detail residuals
or an edge mask. They are transient display state, excluded from history,
autosave, presets and export. Modifier/gesture release, app deactivation, photo
transitions and export preparation restore the normal preview.

Catalog migration 4→5 upgrades edit schema 3→4 with neutral parameters. It preserves
Curve/HSL children, previous correction flags, selection, metadata and revision/
timestamp. Invalid old rows abort the entire migration; unsupported future
catalogs are refused. Older Apertone builds cannot open the upgraded catalog.

Detail is the fifth subset for preset/Copy/Paste/Sync. Preset/collection schema 2
reads schema 1 while rejecting future versions and unknown fields. Old presets do
not acquire absent Detail groups. Batch before/after/conflict checks include all
new fields, preserving excluded groups/photos with atomic Undo/Redo.

Acceptance covers constants and signed/HDR values, edges, flat noise, radius
extent/preview scale, orientations, validation and mid-pass cancellation; migration
preservation/rollback and neutral pixels; all 31 preset subsets and legacy reads;
actual QML pressed/moved/Option bindings, autosave/history/export isolation, Detail
Copy/Paste/presets and QML batch Sync/Undo; restart-identical PNG/JPEG/TIFF16 and ICC.
Additional local checks use a real Nikon D7500 for full-resolution TIFF16 and an
actual 0.4.2 application for byte-identical neutral PNG across migration. No private
photo or path is published. Local verification passed: all 17 CTests, the additional real RAW and old-binary
compatibility checks, clang-format and git diff checks. The completed local
clang-tidy log contains no errors; exact-head macOS CI is required before merge.

Noise reduction, true 1:1, export-specific output sharpening, representative-image
quality tuning and 100 MP performance remain later work. No manual native UI
inspection or equivalence to Lightroom is claimed. See [ADR 0011](adr/0011-detail-sharpening.md).
The frozen 0.4.0 candidate and its signing requirement are unchanged.
