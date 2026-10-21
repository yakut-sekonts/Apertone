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

- Status: **reproduced synthetic failures resolved by v3 replacement; cross-platform CI retest pending**.
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

- Status: **reproduced synthetic failures resolved by v3 replacement; photographic acceptance remains open**.
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

- Status: **closed for the reported build failure — local and Xcode 16.4 CI PASS**.
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

### DRUNet and CI follow-up — 2026-10-21

- BUG-036/037: NAFNet was replaced with hash-pinned MIT DRUNet color and a
  per-tile Haar noise estimate. No darkness cutoff, input suppression or relaxed
  quality/numerical threshold was introduced. The fixed source-quality cases
  pass 7/7; native CPU and ALL comparisons pass 11/11 each. The native outputs
  additionally pass the same analytical-clean quality oracle. See
  [source quality](qualification/2026-10-21/denoise-quality-v3.json),
  [CPU](qualification/2026-10-21/model-numerics-v3-cpu.json) and
  [ALL](qualification/2026-10-21/model-numerics-v3-all.json).
- The replacement resolves these exact synthetic reproductions only. Real
  camera noise, fine texture, color and per-tile estimate consistency still
  require photographic/runtime qualification; ALL does not prove device placement.
- BUG-038: [CI run 39616268298](https://github.com/yakut-sekonts/Luma/actions/runs/39616268298)
  built successfully with Xcode 16.4 and passed the diagnostic lifecycle test
  as part of unit/integration. This does not qualify all crash-reporter paths.

## LUMA-BUG-039 — UX import fixtures target the retired LibraryPane properties

- Status: **test harness corrected; four affected offscreen scenarios PASS; full CI rerun pending**.
- CI after the build fix reports 180 UX failures, frequently at unavailable
  `library.importStart`. Since advanced import, `sources` and `destination`
  belong to `library.importDialog`; old fixtures wrote unused dynamic properties
  on `library.pane`, leaving Start disabled.
- Chooser-result fixtures and selection snapshots now use the actual dialog.
  The shared import helpers assert successful property assignment. Subsequent
  import, selection, edits and export continue through production UI controls.
  Native file-panel selection remains an explicit untested fixture boundary.
- The previous failing CI record is retained. This cause does not establish
  that every one of its 180 failures will pass after the fixture correction.

## LUMA-BUG-040 — UX preset fixtures omit new groups and DiagnosticsController

- Status: **source fixtures corrected; affected offscreen cases PASS; full/native retest pending**.
- The old Light-only selection unchecked seven groups but left the four new
  creative groups enabled. Shared fixture selection now covers the current
  group mask, scrolls controls into view, clicks them and asserts the exact mask.
  Localization coverage includes all 12 labels; its native candidate has not
  been rerun here.
- The preset metadata scenario additionally clicked the recapture checkbox
  below the clipped viewport. Actual scrolling and checked-state acknowledgement
  fix the setup without changing the save/recapture/UUID invariants. Its initial
  failure is retained and its affected retest passes.
- The UX context now supplies the real DiagnosticsController with a disabled
  DiagnosticSession, matching unattended startup. This removes the observed
  undefined-context QML warnings; it does not qualify crash capture or diagnostics UI.
- Evidence: [follow-up](qualification/2026-10-21/drunet-ux-followup.json).

## LUMA-BUG-041 — Per-tile noise estimate mistakes texture for Gaussian noise

- Status: **exact bounded reproductions fixed and retested; full photographic acceptance remains open**.
- With seeded sRGB sigma 0.01, bundle v3 over-smooths the portrait and landscape
  fixtures. The portrait's blue error exceeds the noisy input; the landscape
  also worsens gradient and tile-seam errors. Four of six noisy photographic
  cases pass; [the original FAIL report](qualification/2026-10-21/denoise-photographs-v3-failure.json)
  is retained. All nine source/native comparisons already passed, isolating a
  source-quality defect rather than a Core ML numerical conversion error.
- Bundle v4 uses the lower median of absolute Haar HH coefficients divided by
  the Gaussian calibration constant. The estimator is resistant to sparse
  texture/edge outliers and uses exportable topk/amax. Weights, halo, domain
  preservation and all quality/numerical criteria remain unchanged.
- [Native photographic retest](qualification/2026-10-21/denoise-photographs-v4.json):
  **6/6 noisy cases PASS**, **9/9 tiled source comparisons PASS**. Three unchanged
  photographs are observational controls, not additional quality passes.
  Amount 0/0.5/1, alpha, unchanged input, progress, cancellation before/during
  processing and rejected invalid amounts pass with the actual provider.
- The synthetic source gate remains 7/7 PASS; CPU and ALL comparisons remain
  11/11 PASS each. Thirteen adaptation tests, seven synthetic-oracle tests and
  six photographic-oracle tests pass. Eight affected CTest regressions and
  the application build pass. CI and release now require the photographic gate.
- This does not qualify real sensor noise, full-size RAW, every texture, Sky,
  fallback device placement, resource/load behavior or signed installation.
  See [the follow-up evidence](qualification/2026-10-21/denoise-photographic-followup.json).
