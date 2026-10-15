# Phase 0.5 acceptance

Source 0.5.1 completes the unconditional Local Adjustments roadmap scope in one
feature PR for Issue #44. This is feature acceptance, not a published release or
the subsequent standalone regression campaign requested by the human.

## Delivered scope

- Linear and Radial gradients, Brush size/feather/stroke opacity/erase.
- Luminance Range and Color Range with asynchronous on-photo reference sampling.
- Ordered Add/Subtract/Intersect primitives, enable/invert/reorder/delete.
- Eight local Light/Color controls, mask opacity/inversion and viewing-only overlay.
- Gesture Undo/Redo, persistent complete documents, shared rendering and all eight
  runtime language packages.

Depth Range is explicitly conditional on source depth data. The current pipeline
has no depth channel, so this phase exposes no synthetic Depth control. AI,
Metal, tablet pressure, Auto Mask and mask transfer through presets/batch are not
claimed. See [workflow and numerical definitions](LOCAL_ADJUSTMENTS.md).

## Verification

- `local-mask-render-storage`: independent analytic gradient/brush coverage;
  feather, stroke opacity/erase, physical aspect, fast sparse/dense stroke parity;
  BVH versus independent brute-force segment evaluation; soft boolean order,
  disabled components and inversion; range luminance/color/HDR boundaries and
  fixed pre-local references. CPU preview versus linear export covers color,
  alpha, all EXIF orientations, Crop/Transform and overlay isolation.
- Strict version-1/version-2 serialization, malformed/resource-limit refusal,
  catalog 10→11 legacy upgrade and rollback after a later corrupt row. Persistence
  and batch Undo conflict tests include brush points and composite operands.
- `presets-storage-preview-history`: real QML pointer/control interactions,
  continuous paint/erase, stroke boundary termination, component operations,
  sample Undo/Redo, stale worker cancellation, runtime retranslation, global
  Paste preservation and byte-identical export after restart.
- `complete-mask-native-restart`: actual Main window with transformed EXIF6 RAW,
  Brush/Erase, both sampled ranges, boolean composition, immutable export,
  byte-identical restart, unchanged source SHA and four selection panels in all
  eight languages at the minimum tested 1000-pixel window width.
- `edited-thumbnail-snapshot-cache`: complete composed masks in the saved render;
  canonical mask bytes participate in `edited-v4` cache identities.
- Actual previous 0.5.0 app versus new source: legacy gradient catalog upgraded
  from 10 to 11 and the exported PNG remained byte-identical (licensed landscape,
  Crop/Transform and non-neutral local edits).
- Standalone candidate acceptance repeats complete masks/export/restart while
  Qt and Sparkle developer SDK directories are temporarily unavailable.

Build, CTest, clang-format, clang-tidy and exact-head macOS CI gate integration.
The optional private Nikon fixture may be skipped when not supplied; synthetic
RAW and licensed public image coverage remain mandatory. Public release packaging,
notarization status and the separate regression campaign are the next step.
