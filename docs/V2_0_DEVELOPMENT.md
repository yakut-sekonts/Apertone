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
parity. The release candidate also passes real sensor decode, Develop render,
deterministic export, catalog integrity and source-immutability checks for Canon
CR3/CR2, Sony ARW, Nikon NEF, Pentax PEF, Panasonic RW2, OM System ORF and a
camera-native Ricoh DNG. The CC0 assets, SHA-256 identities and non-degenerate
render metrics are recorded in the
[2.0 real RAW qualification](qualification/2026-11-02/real-raw-2.0.json).

## Intelligent Library

Scoring remains local and inspectable. It may use focus, exposure, blur,
black-frame and reliable local Vision observations. A missing or low-confidence
eye observation is represented as unavailable rather than as a negative score.
Exact duplicates use content identity. Visual similarity must be bounded,
versioned and reviewable before it can influence grouping. Automatic deletion
is outside this block.

Face capture quality and eye openness are optional observations. The scorer
renormalizes its weights over the metrics that Vision actually returned; an
unavailable landmark or capture-quality result cannot create a negative reason
or reduce a photo's score. This contract advances the local culling provider
identity so old and new observations cannot be mixed.

The latest completed import exposes byte-for-byte duplicates as a review list
showing both the skipped source and the retained catalog original. Review never
deletes either file. Visual grouping remains provider-versioned and is used for
bounded burst suggestions only; it is not presented as exact identity.

## Persistent Workflow

History and named snapshots survive restart without copying originals. Catalog
migrations create a recoverable backup, are transactional and reject future
schemas. Repair, relink and sidecar reconciliation preserve edits and expose
conflicts rather than choosing a winner silently. Recovery must be idempotent.

Catalog schema 23 stores at most 200 Undo steps plus the current per-photo state.
The edit document, chronological history and Undo/Redo cursor commit in one
SQLite transaction. A debounce that fires during a drag stores the provisional
value with its gesture base, so an interrupted gesture restores as one Undo step.
History documents are bounded individually and as a timeline; corrupt, duplicate
or mismatched state identities are rejected. Batch changes intentionally clear
the affected per-photo timelines, while named versions remain independent.

## Detail and Resolution

Every operation records its provider, algorithm/model version and input/output
contract. `Raw Details` and deterministic 2x derived resolution may ship without
an ML model when their algorithm and limits are explicit. `AI` or `Super
Resolution` naming requires redistributable bundled weights, an audited license,
numerical/runtime checks and a deterministic fallback. No photo leaves the Mac.

The implemented derived-detail path renders the saved Develop document to
scene-linear sRGB and creates a managed Float DNG. `Raw Details` is available only
for an original Bayer RAW and applies bounded edge-aware microcontrast at the
original dimensions. `2× Detail` applies the same detail pass followed by a
deterministic separable Lanczos-3 resampler with straight-alpha filtering. It
rejects inputs above 25 MP before allocating the 2× surface because the shared
derived-image contract is capped at 100 MP. `Lens Blur Recovery` is a bounded
first-order inverse of a Gaussian point-spread function; it is explicitly a
local deconvolution approximation, not an ML reconstruction. `Color Moiré
Correction` suppresses high-frequency chroma oscillation only where local
luminance is smooth and preserves luminance and alpha.

`Auto noise profile` measures luminance and chroma residuals in locally flat,
covered scene-linear preview regions. A bounded ISO prior makes the result
conservative when measurement is weak. Camera identity is not presented as a
factory calibration profile. The recommendation updates the existing editable
luminance and color noise controls in one Undo step.

The Develop comparison renders the same frozen asset twice: once with a neutral
edit document and once with the active edit document. The split view shares
zoom and pan, supports Fit and full-resolution 100% rendering, and never mutates
the catalog asset.

Derived XMP now uses one validated provenance schema for Raw Details, 2× Detail,
Lens Blur Recovery, Color Moiré Correction, HDR, Panorama and AI Denoise. It
records provider and provider version,
algorithm and algorithm version, model identity when applicable, source IDs and
checksums, parameters, execution location, determinism, and exact linear input
and output contracts. The managed-file journal remains the publication boundary:
cancellation before install leaves no result, while an installed result is
recovered and registered after interruption.

## Final gate

Before tagging `v2.0.0`, the combined branch must pass catalog migration and
rollback tests, CPU/Metal parity, real-camera color and detail fixtures,
background-job cancellation/restart, update compatibility from the latest
stable release, packaging and artifact verification. The published release must
contain the DMG, ZIP, `appcast.xml`, `SHA256SUMS` and `BUILD_INFO.json`, and the
Sparkle update must resolve to the exact tagged archive.
