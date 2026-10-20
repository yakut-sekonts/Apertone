# Phase 0.6 implementation

Status: **in progress — source only, no build, tests or QA executed**.
Development branch: `codex/1.0-development`. The source has advanced to 0.7.0;
the published stable release remains 0.5.1. No release tag or installer is
created from this implementation checkpoint.

## Source changes

- `ByteLruCache` retains immutable values under a byte budget. Eviction releases
  only cache references, not values still owned by active jobs or the viewport.
  Generation tokens prevent work started before a purge from repopulating it.
- `SessionService` caches decoded 2560/1024 previews by source checksum and
  decoder version. Each open still hashes the source and reads current catalog
  edits. A separate bounded cache supports on-demand full-resolution decoding.
- Rendered preview identity includes asset/checksum, serialized settings and
  masks, requested resolution, display ICC profile, diagnostic/overlay state,
  backend choice and pipeline version.
- Ordinary interactions first request a 1024 preview and refine after settling.
  Releasing a gesture requests final quality. Geometry and sampling tools retain
  their existing stable canvas resolution.
- The 100% viewport requests full decoded pixels and maps one output pixel to
  one display pixel using the screen DPR. It supports panning and scrollbars.
  Unsupported full-resolution geometry or render failure returns to Fit with
  an explicit error. Full-frame decode/render still uses the existing 100 MP
  guard; viewport-tiled processing is not implemented.
- Preview preferences persist a 64–2048 MiB cache budget, default 256 MiB, and
  Metal enablement. Source caches receive 75% and rendered output 25%.
  This is a retained-cache budget, **not a cap on total process RSS**.
- A macOS memory-pressure dispatch source evicts optional caches, rejects
  pending cache insertions, exits 100% and replaces it with a Fit preview.
  Cache retention resumes when the OS reports normal pressure.
- Metal accelerates WB, camera-to-sRGB, Exposure, Tone, Saturation, Vibrance,
  parametric/point Curves and all eight HSL bands. The point-curve LUTs and
  extrapolation slopes come from the CPU pipeline's PCHIP tables.
  It uses precise math and bounded 262144-pixel staging chunks with cancellation
  between submissions. Initialization/allocation failures and command failure
  use the CPU reference. Detail, local masks, geometry,
  optics and ICC conversion remain shared CPU stages. Export remains CPU.

## Remaining work

- Further independent GPU passes remain optional extensions; current GPU
  coverage includes the complete global Light/Color/Curve/HSL Develop stage.
- Assess whether full-resolution tiled processing is required on 8 GB machines
  during the final campaign; do not claim a total memory ceiling or latency.
- At pre-1.0 qualification: compile all targets, verify image equivalence with
  CPU including HDR/negative values, alpha, masks, disabled corrections and ICC;
  exercise progressive cancellation, photo/profile switches, 100%/Fit, cache
  eviction, pressure recovery and all language packages.
- Include 8/16 GB latency/resource and large-catalog campaigns at final QA.
  No equivalence, performance, memory or UX results are claimed at this checkpoint.

The dedicated development branch is excluded from intermediate macOS CI.
Qualification must use a separate candidate branch so the existing mandatory
CI and release workflows run before publication. Historical 0.5.1 gaps and
new reports remain in [the development backlog](DEVELOPMENT_BACKLOG.md).

Metal implementation reference:
[Apple — Performing calculations on a GPU](https://developer.apple.com/documentation/metal/performing-calculations-on-a-gpu).
