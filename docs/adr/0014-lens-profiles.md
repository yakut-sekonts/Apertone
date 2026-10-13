# 0014 — Calibrated optical correction

Status: accepted for the Lens Corrections substage of 0.4 (§25).

Use the unmodified XML database from Lensfun v0.3.4, verified by SHA256,
converted deterministically to a separate generated data header at build time.
The data and generated representation retain CC BY-SA 3.0 attribution; ship
the original data and notices in the bundle. Luma's independent implementation
does not compile, copy or link Lensfun library code. No GLib dependency.

Support rectilinear poly3/poly5/PTLens distortion, linear/poly3 lateral CA,
and PA vignetting. Convert pixel centers to calibration sensor coordinates
using camera crop factor, calibration crop factor/aspect/center. Match known
camera and lens aliases conservatively and within compatible mounts; ambiguous
matches remain unresolved. Manual profile IDs are stable hashes of profile
identity. Unknown camera, unknown profile and missing/out-of-range calibration
are visible; they never acquire guessed coefficients. Interpolate within
calibrated focal/aperture bounds. Focus distance is not currently in the
catalog: use the farthest calibrated distance and state this approximation.
This layer does not claim numerical equivalence to Lensfun's interpolation.

Apply vignette gain and per-channel inverse optical mapping to sensor-linear
RGB before camera color conversion, Develop and Detail. Crop/Straighten follows
Detail. Both preview and export use the same pass. Active correction owns one
additional float RGB image; inactive correction returns the original image
without copying. Bilinear sampling preserves signed HDR. Safe inward scaling
keeps all channel lookup positions inside the source; reject pathological
non-monotonic models. Disable/zero/unavailable corrections preserve exact pixels.

Persist profile ID, distortion/vignetting amounts and CA switch. The profile
enable bit defaults off, including migration, to preserve old exports.
Catalog 7→8 / edit 6→7 migration is transactional; presets schema 5 reads
1–4 and adds an Optics subset to Copy/Paste/Sync and batch Undo/conflicts.
No automatic download or opaque per-photo profile changes. The pinned database
version determines automatic resolution; an explicit manual selection persists.

Tests cover equations and channels, real profile selection and ambiguity,
sensor/resolution normalization, boundaries/cancellation and exact neutral
pixels, persistence/migration/rollback, QML gesture/history, preset subsets,
batch Undo and restart-identical exports. Perspective Transform is separate.
