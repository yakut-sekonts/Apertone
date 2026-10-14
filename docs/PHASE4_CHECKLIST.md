# Phase 0.4 completion boundary

Human instruction: finish phase 0.4 according to ROADMAP.md, report completion,
and stop before phase 0.5 for adjustments. Local Adjustments must not start.

- [x] Curves/HSL and targeted tools.
- [x] User presets and selected-subset Copy/Paste.
- [x] Atomic multi-photo presets/Paste/Sync and conflict-aware session Undo/Redo.
- [x] Detail sharpening and manual luminance/color noise reduction.
- [x] Crop, ratios, straighten, flips and transient composition guides.
- [x] Calibrated Optics, manual/Auto profiles, distortion/vignette amounts and CA.
- [x] Perspective Transform (§26): manual Rotate/Horizontal/Vertical/Aspect/Scale/
  X/Y Offset, Auto with confidence refusal, Guided vertical/horizontal line pairs.
- [ ] Ready-made filters: declarative presets, hover, one Undo, preserve excluded
  groups, representative distributable licensed image fixtures and validation.
- [ ] Edited thumbnails: correct edit-dependent cache identity, revision updates
  in grid/filmstrip after autosave/batch changes, stale-result isolation and restart.
- [ ] Advanced export (§52): JPEG/PNG/TIFF 8/16/compression/DNG, named color spaces,
  resize modes, output sharpening, metadata/location control, text/image watermark,
  destination/naming/overwrite and reusable export settings.
- [ ] Export queue (§53): filename, progress, completed/total, errors, pause/resume/
  cancel and source integrity; file writes remain atomic.
- [ ] Final phase-0.4 validation and standalone macOS bundle; roadmap updated to
  completed, no phase-0.5 implementation.

Each implementation substage requires unit/integration/QML tests appropriate to
its behavior, exact-head macOS CI and reviewable Git history. Geometry/edit changes
must participate in preview/export, persistence, migration, presets and batch Undo.
Old neutral renders and original RAW hashes must remain unchanged. Optional WebP/
AVIF, Metal/progressive rendering, AI and stabilization-scale catalogs are outside
this boundary. A new public signed release requires its separate release gates;
development bundles must not be represented as notarized releases.
