# Phase 0.7 source development

Status: **in progress, unbuilt and unqualified**. Source development is on
`codex/1.0-development`; stable remains 0.5.1. No installer, tag or release is
produced. All execution and QA wait for final pre-1.0 qualification.

## Local masks

Apple Vision creates Subject, Background and point-selected foreground Object
masks on a maximum-1024-pixel upright reference. Results are mapped back to the
post-optics, pre-crop source canvas and persisted as immutable byte coverage.
They use the existing adjustments, overlay, Add/Subtract/Intersect, Undo/Redo,
preview, thumbnail and export pipeline. Image/edit changes invalidate pending
requests. Cancel, Escape, close and memory pressure cancel pending AI work.

The bitmap budget is 4 Mi pixels per complete document, shared across immutable
history snapshots; serialization is strict canonical hex in mask document v3.
Catalog migration 12 raises the stored document budget to 16 MiB and accepts
legacy v1/v2 documents. Vision does not provide Sky in this implementation.
The separate optional Core ML Sky adapter accepts a local compiled model and
returns probability coverage through the same mask/Undo path. Sky stays disabled
until a model path is configured; actual model validation occurs on the worker.

Sky contract: Float32 `image` `[1,3,H,W]` to `sky` `[1,1,H,W]`, matching static
64–1024 dimensions, top-left sRGB RGB input in [0,1], bilinear stretch resizing,
and finite sky probabilities in [0,1]. Creator metadata must declare
`luma.sky.schema=1`, `luma.sky.color=srgb`, `luma.sky.resize=stretch`.
Compute-unit failure retries CPU. Prediction is synchronous and cancellation
is checked before/after it. No Sky weights are bundled or downloaded.

## Retouch

Clone, Heal and Remove persist ordered strokes in `edit_retouch` (migration 13).
Brush radius, feather, opacity, enabled state, path and source translation are
non-destructive. Option-click chooses/repositions a donor. A completed stroke
is one Undo; controls retain gesture history. Global presets/batch operations
preserve each target's retouch, including conflict-aware batch history.

Retouch runs after optics and before Develop/Detail/local adjustments in all
render paths. Clone reads a snapshot of the donor, so overlapping strokes do
not recursively smear. Heal matches the surrounding ring's mean color.
Remove searches a deterministic donor lattice, refines the offset, and saves
it once; export does not perform a new search. It is intended for small objects
with usable surrounding texture. This is patch-based removal, not generative
fill, Poisson blending or a neural inpainting model. The editor reports failure
when no disjoint donor exists; a source can be chosen manually.

## Optional AI Denoise

The Core ML adapter accepts a user-selected local `.mlmodelc`. It runs on
available compute units with CPU fallback, Float32 linear RGB tensors, reflected
tile borders and a 16-pixel halo. Amount blends source and prediction. Alpha,
finite samples and tensor shape/stride bounds are handled explicitly. Cancellation
is checked between predictions; a single prediction is synchronous.

Model contract: one Float32 input `noisy` and output `denoised`, both static
`[1,3,H,W]`, dimensions 64–1024. Creator-defined metadata:

- `luma.denoise.schema = 1`
- `luma.denoise.color = linear-srgb`
- `luma.denoise.halo = 16`

No trained weights are bundled or downloaded. Choosing a model in Settings
records a local path; loading/contract validation occurs on the worker when
Denoise runs. **Selection, licensing, packaging and qualification of a default
model remain open.** An adapter alone is not a qualified denoise product.

Denoise takes a neutral upright RAW/Linear DNG and creates a separate unedited
Float32 Linear DNG in the catalog's `.luma-derived` directory. Current Develop
edits are not copied to the derivative. The original remains unchanged. Managed
output uses a temporary file, fsync and no-replace installation. Provenance XMP
records the source asset/checksum and operation without its original filesystem
path. Float samples preserve negative values and HDR highlights. Default export
continues to use the existing integer16 DNG mode.

## Local semantic search

An explicit Library dialog enables/resumes indexing and searches up to 100
ranked matches. Vision image classifications are encoded with the system's
English sentence model and combined by confidence. The backend is label-based,
**not a CLIP encoder**; it can miss details or relationships not represented by
recognized categories. Queries are English even when the UI uses another language.
Model availability and retrieval quality remain unqualified.

Normalized embeddings are stored locally with asset checksum and provider/model
revision in migration 14. Searches stream the index into a bounded result heap.
Indexing is opt-in, resumable, cancellation-aware and records per-photo failures.
Disabling clears results, persists disabled state and serially deletes stored
embeddings. Startup retries deletion for a disabled index. SQLite deletion is
logical deletion, not forensic secure erasure of old pages/backups.

## Migration and outstanding work

Opening an older catalog first creates a SQLite online-backup snapshot in its
`.luma-backups` directory; migration is refused if backup creation fails.
The upgraded catalog cannot be opened by 0.5.1. Restore the pre-upgrade backup
for downgrade; do not overwrite it with a new-schema catalog.

Remaining source decisions: denoise/Sky weights and delivery strategy, and stronger
semantic image/text encoding if category search is insufficient. Derivative
recovery/registration is now journaled in phase 0.8; HDR/panorama, metadata/XMP,
relink and recovery UI have initial source implementations in phases 0.8–0.9.
Full model/image quality, orientation,
crop/lens alignment, migration, cancel/race, memory, native UX and load checks
are all deferred; none is marked PASS.

Platform references:
[Vision instance masks](https://developer.apple.com/documentation/vision/vninstancemaskobservation),
[Core ML](https://developer.apple.com/documentation/coreml/mlmodel),
[image classification](https://developer.apple.com/documentation/vision/vnclassifyimagerequest),
[sentence embeddings](https://developer.apple.com/documentation/naturallanguage/nlembedding).
