# Manual noise reduction — 0.4.4 development

[Issue #18](https://github.com/yakut-sekonts/Luma/issues/18) implements the second
Detail substage from requirements §23. All six 0–100 controls, independent enable
states, gesture Undo/Redo and autosave are included. Luminance Amount and Color
Amount default to zero, preserving the preceding pixels without spatial buffers.

The shared CPU path runs DevelopPipeline → NoiseReductionPass → SharpeningPass →
ICC/quantization in preview, PNG/JPEG and TIFF16. Separable joint bilateral
filtering uses original luminance/chroma guides in both directions, replicated
borders and at most 19 horizontal rows. Color channels share range weights.
Luminance filtering preserves chroma differences; color filtering preserves
luminance. Amount blends, Detail narrows range tolerance, Contrast retains
stronger luminance residuals, Smoothness increases color spatial radius.
Signed/HDR data is not clipped in this pass. Preview scales the spatial radius.

Temporary Option diagnostics display luminance or chroma at neutral luminance,
bypass sharpening and are excluded from saved edits, history and export. Modifier,
gesture, application and editor transitions restore normal preview. Detail remains
the fifth subset for presets, Copy/Paste and batch Sync/Undo. Batch conflicts and
rollback compare all noise columns, including independent enable flags.

Catalog 5→6/edit schema 4→5 preserve old settings, sharpening, Curve/HSL children,
selection, metadata and revisions/timestamps. Failure rolls back the entire
migration. Older builds refuse catalog 6. Preset/collection schema 3 reads 1/2;
legacy schema-2 Detail initializes absent noise to neutral defaults, so applying
an old Detail preset resets noise. Presets without Detail preserve noise.
Unknown versions/fields are rejected without replacing stored data.

Acceptance covers noise variance, strong luminance/chroma edges, six control
semantics, Amount blending, component isolation, constants/signed/HDR values,
1-column input, radius scaling, all orientations and noise→sharpening composition,
validation and cancellation in each phase. Integration covers migration
preservation/rollback and neutral pixels, restart-identical PNG/JPEG/TIFF16 with
ICC, all 31 preset subsets and legacy data, six QML gesture bindings/Option views,
enable switches, export isolation and atomic batch Sync/Undo. Additional local
checks use a real Nikon D7500 for full-size TIFF16 with both Detail stages and the
actual 0.4.3 bundle for neutral migration/old-reader refusal. No private photo/path
is published. All 19 local CTests passed; the 5 affected tests passed again after removing
redundant pointwise processing. The additional real RAW/old-binary checks and
clang-format/full clang-tidy passed. Changed rendering sources were rechecked.
Exact-head macOS CI is required before merge.

No manual native UI inspection, Adobe equivalence, representative-photo tuning,
100 MP performance, AI denoise, Metal or true 1:1 is claimed. Additional memory
is a three-component Float32 image plus scalar outputs/row ring; sharpening adds
its two planes when enabled. See [ADR 0012](adr/0012-detail-noise-reduction.md).
The frozen 0.4.0 candidate/signing requirement is unchanged.
