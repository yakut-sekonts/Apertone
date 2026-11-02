# Apertone 2.0 development

## Release policy

Apertone 2.0 is one development cycle built on the published 1.6 release. The
1.7, 1.8 and 1.9 labels identify internal blocks only. They do not receive tags,
GitHub Releases or Sparkle appcast entries. The next public update is `v2.0.0`.

Each block uses separate commits and targeted checks for the changed component
and its direct dependencies. The complete product gate runs once the combined
scope is feature-complete. A passing test for one block is not a release claim.

## Camera Fidelity

The Bayer path normalizes with validated per-channel sensor limits and performs
bounded camera-linear highlight reconstruction before White Balance and the
camera transform. DNG camera profiles retain two standard calibration slots,
interpolate matrices in reciprocal color temperature and resolve the transform
again whenever the selected White Balance changes.

The DNG transform follows the public DNG 1.7.1 processing model:

- `XYZtoCamera = AnalogBalance * CameraCalibration * ColorMatrix`;
- without `ForwardMatrix`, invert `XYZtoCamera` and apply linear Bradford
  adaptation from the selected white point to D50;
- with `ForwardMatrix`, compute the reference neutral and the specified
  `ForwardMatrix * D * Inverse(AnalogBalance * CameraCalibration)` transform;
- factor existing camera gains out of the matrix because Apertone applies them
  explicitly before the shared CPU/Metal color matrix.

Synthetic fixtures prove parsing, endpoint selection, reciprocal-temperature
interpolation, As Shot behavior, failure on singular metadata and CPU/Metal
parity. Release qualification still requires representative real RAW files from
Canon, Sony, Nikon, Pentax, Panasonic, OM/Olympus and a native DNG camera.

## Intelligent Library

Scoring remains local and inspectable. It may use focus, exposure, blur,
black-frame and reliable local Vision observations. A missing or low-confidence
eye observation is represented as unavailable rather than as a negative score.
Exact duplicates use content identity. Visual similarity must be bounded,
versioned and reviewable before it can influence grouping. Automatic deletion
is outside this block.

## Persistent Workflow

History and named snapshots survive restart without copying originals. Catalog
migrations create a recoverable backup, are transactional and reject future
schemas. Repair, relink and sidecar reconciliation preserve edits and expose
conflicts rather than choosing a winner silently. Recovery must be idempotent.

## Detail and Resolution

Every operation records its provider, algorithm/model version and input/output
contract. `Raw Details` and deterministic 2x derived resolution may ship without
an ML model when their algorithm and limits are explicit. `AI` or `Super
Resolution` naming requires redistributable bundled weights, an audited license,
numerical/runtime checks and a deterministic fallback. No photo leaves the Mac.

## Final gate

Before tagging `v2.0.0`, the combined branch must pass catalog migration and
rollback tests, CPU/Metal parity, real-camera color and detail fixtures,
background-job cancellation/restart, update compatibility from the latest
stable release, packaging and artifact verification. The published release must
contain the DMG, ZIP, `appcast.xml`, `SHA256SUMS` and `BUILD_INFO.json`, and the
Sparkle update must resolve to the exact tagged archive.
