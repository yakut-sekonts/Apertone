# Development backlog toward 1.0.0

Implementation continues without intermediate QA by user direction on
2026-10-20. Open reports are not independently reproduced or qualified.
All deferred checks and fixes must be covered in the final pre-release campaign.
Final qualification started on 2026-10-21; newly reproduced failures below retain
their actual execution status rather than the earlier development-only status.
The 23 accepted 0.5.1 qualification gaps remain tracked in
[Issue #48](https://github.com/yakut-sekonts/Luma/issues/48) and
[the release record](releases/v0.5.1-quality.md).

## LUMA-BUG-035 — Library scroll returns to the beginning on photo selection

- GitHub: [Issue #49](https://github.com/yakut-sekonts/Luma/issues/49).
- Status: **open — source fix implemented; confirmation deferred to final QA**.
- Reported: 2026-10-20, after installation of the 0.5.1 release.
  The exact running build has not been independently established.
- Reproduction supplied by the user: import photographs, scroll down through
  the Library, then select a photograph.
- Actual: the list jumps back to the beginning after every selection.
- Expected: selecting a photograph preserves the visible asset anchor and its
  offset in the viewport; the selected photo opens without resetting scroll.
- Evidence supplied: local screen recording
  `Запись экрана — 2026-10-20 в 17.33.02.mov`. The recording has not been
  independently analyzed; the description above records the user's report.
- Likely investigation area, not an established cause: Library selection,
  `photoOpened`, model reload/reset, and viewport restoration.
- Source change: opening an asset already present in a loaded metadata page
  updates Active/Selected roles without resetting the model. Card clicks set
  the view's current index before transferring focus; the grid starts with no
  implicit current item. This has not been run or independently confirmed.
- Final qualification must cover click selection after deep scrolling, repeated
  selection, paged results, sort/filter combinations and asynchronous photo load.

## LUMA-BUG-036 — Compiled Denoise differs from its source network

- Status: **open — native downsampling error fixed; dark-input numerical failures remain**.
- Environment: Apple Silicon, macOS 27.0.1, Xcode 27.1; pinned source weights,
  coremltools 9.0 conversion, Float32 ML Program and native CPU_ONLY prediction.
- Three synthetic cases (noisy gradient, black, negative/HDR gradient) exceed
  the fixed max-absolute-error 0.0005 / RMSE 0.00005 limits. Maximum observed
  error is 0.2002. Source Float32 versus Float64 on black differs by at most
  2.87e-5, so ordinary source precision does not explain the Core ML discrepancy.
- Disabling layer-norm fusion did not fix the black-input discrepancy. Early
  encoder-prefix comparisons locate the first large deviation at the stride-2
  downsampling convolution. Disabling all conversion passes also failed.
  An exploratory equivalent split/1×1 convolution reduced the black-input error
  to 0.000757 but still exceeded the fixed limits; no experimental adaptation was
  added to the production model recipe. Switching to the neural-network backend
  also remained outside limits (black max 0.000735 / RMSE 0.0000649). Equivalent
  odd-kernel and pointwise-linear experiments did not meet the gate either.
  These diagnostics do not justify changing weights or tolerances.
- Evidence: [numerical report](qualification/2026-10-21/model-numerics.json), `final-denoise-float64.log`,
  `final-denoise-diagnostic-1.log`, and `final-denoise-layers-*.log`.
- Sky passes the same synthetic CPU comparison. This does not establish its
  photographic quality, no-sky behavior, GPU/ANE results or memory limits.

### Follow-up — 2026-10-21

- The same v1 recipe on macOS 15.7.9 / Xcode 16.4 passed the ordinary gradient
  and HDR cases, but still failed black. Evidence: [CI run](https://github.com/yakut-sekonts/Luma/actions/runs/39608476171)
  and [its report](qualification/2026-10-21/model-numerics-ci-v1.json).
- On macOS 27, an isolated 32→64 channel 2×2/stride-2 convolution reproduced the
  boundary error (max 0.03495), independently of the rest of NAFNet. Adaptation
  v2 uses four spatial slices and a 1×1 convolution with the same weights/bias.
  Float64 algebraic, channel/phase-order and odd-size tests pass; the original
  checkpoint layout is preserved. The normal-gradient error fell to 2.45e-6.
- Uniform RGB tiles now pass through exactly. This prevents source-network
  artifacts on zero-variation colors and has no darkness threshold. The tests
  include almost-black noise and a single nonzero pixel so this guard cannot
  hide the remaining nonuniform dark failures.
- [v2 CPU](qualification/2026-10-21/model-numerics-v2-cpu.json): 9/11 cases pass;
  dark noise and black with an impulse still fail. [v2 ALL](qualification/2026-10-21/model-numerics-v2-all.json):
  10/11 pass; dark noise still fails. Max error limits remain 0.0005 / RMSE 0.00005.
  ALL denotes the requested policy, not proof of GPU/ANE execution.
- Selectively disabling convolution/arithmetic fusion did not pass the dark
  impulse comparison. Those experimental converter changes are not retained.

## LUMA-BUG-037 — Source Denoise invents bright structure on nonuniform dark input

- Status: **open — reproduced; blocks photographic model acceptance**.
- This occurs in the pinned source network before Core ML conversion. A linear
  dark ramp plus noise, whose maximum is 0.00109, produces values up to 0.642.
  A black tile with one 0.001 impulse produces values up to 1.0 and mean 0.0651.
- Evidence: [source-output metrics](qualification/2026-10-21/source-dark-sanity-v2.json),
  generated from the same deterministic inputs as the retained numerical checker.
- The exact-uniform guard in v2 only protects tiles with no spatial variation.
  It deliberately does not skip these nonuniform cases or redefine them as PASS.
- The bundled denoiser remains unqualified. Resolve the source-model behavior
  and verify photographic shadow/detail/noise quality before accepting numerical
  agreement as evidence for release. Matching an incorrect source output is not
  sufficient.
- Follow-up: `check-denoise-quality.py` now makes this a mandatory CI/release
  gate against analytical clean images. The v2 source passes 5/7 inputs and
  fails both nonuniform dark cases, in the full image and the runtime crop.
  [Retained report](qualification/2026-10-21/denoise-quality-v2.json).
  Replicate/reflect padding experiments also produced artifacts and were not
  adopted. Weights, conversion recipe and numerical limits remain unchanged.

## LUMA-BUG-038 — Diagnostic writer prevents the supported CI build

- Status: **source fix and local regression PASS; Xcode 16.4 CI retest pending**.
- [CI run](https://github.com/yakut-sekonts/Luma/actions/runs/39612524379) fails
  compiling `DiagnosticSession`: Xcode 16.4's libc++ has no `std::jthread` or
  `std::stop_token`. Local Xcode 27 builds did not expose this incompatibility.
- Replaced the writer with `std::thread` and a stop flag protected by the same
  mutex as its queue. The destructor still wakes, drains and joins the writer.
- A new test exercises immediate shutdown with queued events, event order,
  clean/unclean final markers, disabled logging and symlink-root refusal.
  It uses the production writer and stubs only crash capture/formatting;
  PLCrashReporter, rotation, load and all diagnostic failure modes are not
  qualified by this test.
