# ADR 0012: Shared manual noise reduction before sharpening

## Status

Accepted for the second Detail substage (0.4.4 development).

## Decision

Provide Luminance Amount/Detail/Contrast and Color Amount/Detail/Smoothness,
all in 0–100. Amount defaults to zero; Detail and Color Smoothness default to 50,
Luminance Contrast to zero. Independent enable flags preserve inactive settings.
Manual, deterministic CPU processing follows DevelopPipeline, precedes Sharpening
and ICC/quantization, and is shared by preview, PNG/JPEG and TIFF16.

Use linear-sRGB luminance Y and two opponent differences R-Y/B-Y. A separable
joint bilateral approximation uses spatial and range weights, replicated borders
and bounded horizontal rows. Both chroma channels share a chroma/luminance guide.
Color Detail narrows the chroma range, Smoothness increases the spatial radius.
Luminance Detail narrows its range, Contrast retains stronger local residuals.
Amount blends filtered and original components. Color-only preserves luminance;
luminance-only preserves chroma differences. No signed/HDR clipping occurs here.
Spatial radius scales in reduced previews; Fit is not a substitute for true 1:1.
This is Luma's own filter, not Adobe's algorithm or AI denoise.

An optional developed component buffer and reusable scalar filter buffers are
allocated only for active noise reduction or its diagnostic view. Cancellation
and progress checks occur per row. Color weights are shared across channels.
Zero/disabled noise preserves the old rendering path and pixels. Sharpening
consumes the noise-reduced values rather than applying the point pipeline twice.
Option-drag shows neutral luminance or chroma on neutral luminance; diagnostic
state never enters edits, presets, autosave, history or exports.

Catalog 5→6 transactionally rebuilds the edit parent as schema 5, adding six
parameters and two enable flags while preserving all prior columns, children,
metadata, revisions and selection. Invalid legacy data rolls back the migration.
Older builds refuse catalog 6. Detail remains the fifth settings subset.
Preset schema 3 writes all Detail parameters; schema 1/2 remain readable. Legacy
schema 2 Detail initializes absent noise controls to neutral defaults with flags
enabled. Applying such a Detail preset therefore resets noise to neutral. Missing
Detail groups preserve current noise. Future/unknown fields remain protected.
Batch journals/conflict comparisons include all six parameters.

## Consequences

Noise filtering needs a full three-component Float32 buffer and scalar outputs,
in addition to decoded data and sharpening planes when both are active. Large
image memory and visual tuning on representative licensed photos remain to be
measured. No Metal, AI, 1:1 rendering or new public release is included.

The filtering approach is informed by [Tomasi and Manduchi's bilateral filtering
paper](https://users.cs.duke.edu/~tomasi/courses/vision/handouts/bilateral.pdf);
the separable linear-RGB opponent approximation is a deliberate implementation
choice rather than the full 2D perceptual filter described there.
