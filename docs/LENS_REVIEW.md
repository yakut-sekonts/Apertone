# Lens Corrections — 0.4.6 development

[Issue #22](https://github.com/yakut-sekonts/Apertone/issues/22) implements §25.
Develop adds a searchable manual lens list, conservative Auto identification,
profile enable, Distortion/Vignetting amounts (0–200, calibrated at 100), lateral
Chromatic Aberration switch and per-effect availability. Unknown or ambiguous
metadata never receive guessed coefficients. Camera mount and sensor coverage
constrain manual selection. Missing saved profiles remain visible and inert.

The Lensfun v0.3.4 XML database is SHA256-pinned. A deterministic build-time
conversion creates typed arrays with stable profile IDs; only rectilinear
poly3/poly5/PTLens distortion, linear/poly3 TCA and PA vignetting are supported.
The original XML and generated adaptation retain CC BY-SA 3.0 attribution and
are distributed with their notices in the app. Apertone uses original mathematics
and profile handling; Lensfun library/application/build code is not compiled.
No GLib or new dynamic runtime dependency is added. See ADR 0014.

Per-channel inverse radial mapping and source-sample vignette gain operate in
sensor-linear RGB before WB/matrix mixing, Develop and Detail. Crop/Straighten
remains after Detail. Preview and PNG/JPEG/TIFF16 share the optical pass. Bilinear
sampling preserves signed HDR. Polynomial derivative bounds provide conservative
inward scaling for all channel coordinates over the full sensor; unsafe models
fail visibly. Active optics allocates one float RGB buffer; disabled, zero or
unavailable correction returns the original buffer and exact previous pixels.
Targeted tools sample the corrected input through Crop inverse coordinates.

Distortion/CA interpolate linearly within calibrated focal bounds. PA vignetting
first interpolates aperture at calibrated focal slices, then focal, using the
farthest calibrated distance at each sample. Focus distance is not currently in
the catalog, so this is explicitly an approximation for distant focus.
The interpolation does not claim Lensfun numerical equivalence. Vignette strength
is an exponent of reciprocal attenuation; distortion strength blends radial
coefficients with identity. No calibration extrapolation outside known bounds.

Catalog 7→8 / edit schema 6→7 migration preserves existing settings, child tables,
flags, metadata, selection, revisions and timestamps; profile correction defaults
off. Invalid legacy rows roll back the whole transaction. Old readers refuse
catalog 8. Lens IDs, amounts and CA participate in autosave, gesture Undo/Redo,
preset hover/application and conflict-aware atomic batch Undo. Optics is the
seventh settings subset; schema-5 presets read 1–4 without adding absent groups.

Validation performed locally:

- All 23 CTests, including generated calibrated DNG, migration/rollback, neutral
  byte-identical PNG/JPEG/TIFF16, repeat exports across restart and source hashes.
- Independent equation checks for all supported radial models, diagonal/minimum
  edge normalization, crop factors, sensor-channel CA, source bounds, signed HDR,
  affine-ramp interpolation, cancellation and exact disabled/zero buffers.
- Actual Nikon database aliases, calibration coefficients, focal interpolation,
  missing aperture/out-of-range data, unresolved/missing profiles, and ambiguous
  Canon calibration profiles resolved only by manual selection.
- QML enable/selection/CA/reset, one Undo per gesture, Copy/Paste/preset hover,
  autosave/restart, Original/Crop/export isolation, targeted adjustment Undo,
  all 127 preset subsets and Optics batch Sync/Undo/Redo with excluded settings.
- Actual 0.4.5 app → new catalog neutral PNG compatibility and old-reader refusal.
- Private Nikon D7500 full-resolution TIFF16 with optics + Noise/Sharpening +
  Crop/Straighten, repeat export, disabled-optics difference and untouched NEF.
- Generated database reproducibility, clang-format and clang-tidy.

Exact-head macOS push/PR CI and standalone bundle verification are required
before integration. The existing frozen 0.4.0 release candidate is unchanged;
0.4.6 is a development bundle, not a new signed/notarized public release.
Fisheye projection conversion, longitudinal CA/defringe, perspective Transform,
profile downloads and a focus-distance-aware model remain outside this stage.
