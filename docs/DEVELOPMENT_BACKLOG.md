# Development backlog toward 1.0.0

Implementation continues without intermediate QA by user direction on
2026-10-20. Open reports are not independently reproduced or qualified.
All deferred checks and fixes must be covered in the final pre-release campaign.
Final qualification started on 2026-10-21; newly reproduced failures below retain
their actual execution status rather than the earlier development-only status.
The 23 accepted 0.5.1 qualification gaps remain tracked in
[Issue #48](https://github.com/yakut-sekonts/Luma/issues/48) and
[the release record](releases/v0.5.1-quality.md).

## LUMA-BUG-035 — Filmstrip/Library scroll returns on photo selection

- GitHub: [Issue #49](https://github.com/yakut-sekonts/Luma/issues/49).
- Status: **bounded grid and filmstrip regressions pass offscreen/native; broader qualification remains open**.
- Reported: 2026-10-20, after installation of the 0.5.1 release.
  The recording displays 0.5.1; the exact executable identity is unavailable.
- Reproduction supplied by the user: import photographs, scroll down through
  the Library, then select a photograph.
- Actual: the list jumps back to the beginning after every selection.
- Expected: selecting a photograph preserves the visible asset anchor and its
  offset; opening it and returning from Develop also preserves that viewport.
- Recording reviewed on 2026-10-22: the affected surface is the horizontal
  Develop filmstrip. Two photo switches return it to the initial filename range.
  The earlier Library grid scenario alone did not cover this evidence. See
  [recording observations and hashes](qualification/2026-10-22/scroll-recording.json).
  Original media and extracted frames remain local.
- Source change: opening an asset already present in a loaded metadata page
  updates Active/Selected roles without resetting the model. Card clicks set
  the view's current index before transferring focus; the grid starts with no
  implicit current item. Final qualification additionally reproduced a 10 px
  shift while the grid was hidden after opening a photo. Explicitly retaining
  `contentY` after mouse focus cancels Qt's deferred current-cell reveal without
  disabling arrow-key navigation.
- 2026-10-22: three local offscreen repetitions pass with 176 generated PNGs,
  a metadata-page boundary, two selections, open/return, filename filtering and
  clearing the filter. The oracle checks the actual visible asset/offset,
  transient click motion, absence of model resets and keyboard reveal. Two
  targeted QML mutations are rejected by the expected scroll/reset assertions.
  See [retained failures and retests](qualification/2026-10-22/library-scroll-and-cancel.json).
- Native follow-up on 2026-10-22 passes 3/3 Library runs on Cocoa/Metal, Retina
  DPR 2; neighboring keyboard checks pass 6/6. Source/build provenance and
  originals remain intact, with no QML warnings. See
  [native evidence](qualification/2026-10-22/native-scroll-cancel.json).
- A separate filmstrip regression reproduced a 24.82 px partial-card shift.
  Mouse clicks now retain horizontal position; explicit Left/Right navigation
  reveals the current card after mouse positioning. Final offscreen checks pass
  grid 3/3, filmstrip 3/3, keyboard 6/6 and neighboring selection/scroll 9/9.
  The prior positioning still fails the final anchor oracle. Frame-synchronized
  positions distinguish visible motion from synchronous intermediate property
  changes. See [failures, controls and retests](qualification/2026-10-22/filmstrip-keyboard-followup.json).
- The final native follow-up passes grid 3/3, filmstrip 3/3 and keyboard 6/6 on
  Cocoa/Metal, Retina DPR 2, with intact originals and no QML warnings.
- Physical trackpad/wheel input, other window sizes,
  sort/filter combinations and large catalogs remain part of final qualification. This does
  not establish reproduction of the user's exact installed build or close 1.0 QA.

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

## LUMA-BUG-042 — Add rejects valid existing long filenames

- Status: **bounded reproductions fixed and retested; native retest open**.
- Add without backup incorrectly applied the generated-destination length rule
  to existing source names, rejecting legal long filenames used by VIS001/003.
  It now uses the source path directly. Copy/Move and backup destinations retain
  their length validation. ASCII/UTF-8 boundary unit cases preserve source hashes
  and reopened catalog paths; affected UX scenarios pass three repetitions.
- File descriptor errors now preserve errno and identify the actual failed path;
  the read-only import destination case passes three repetitions.

## LUMA-BUG-043 — Source directory events reset the Library viewport

- Status: **actual watcher reset fixed; bounded unit/offscreen retests pass**.
- Passive model-reset traces isolate delayed source-directory notifications as
  another reset cause after selection/editing. The old reload emptied and rebuilt
  the model, destroying cards and moving the current viewport (related BUG-035).
- A cancellable background check updates only changed Missing roles on cached
  pages, with revision/closing guards. Directory events do not change catalog
  membership or auto-import external edits. Real removal/restoration verifies
  both role changes and zero resets; four affected UX races pass 3/3 each.

## LUMA-BUG-044 — Metal HSL precision near black and after HDR curves

- Status: **near-black calculation fixed; unsupported combined path uses CPU**.
- The independent native oracle reproduced a near-black saturation error and
  excessive Curve/HSL HDR rounding. Stable black/white formulas fix the first.
  Combined Curve/HSL remains outside the fixed FP32 budget and explicitly falls
  back to the unchanged CPU reference, including direct accelerator calls.
- Seven actual Metal cases and two exact fallback cases pass. Numerical limits,
  alpha preservation and exact preset/removable-source UX pixel checks were not
  relaxed. The fallback may reduce preview speed; full load testing remains open.

## LUMA-BUG-045 — Intermittent PhotoCard localization state invariant

- Status: **open; per-field diagnostics added, cause not yet established**.
- After correcting the obsolete cache-key fixture, one of three local runs
  failed the full editor/history/selection/preview invariant. The original
  aggregate failure is retained; it does not identify which field differed.
- Subsequent diagnostic runs pass 10/10 and 20/20, including a run alongside
  clang-tidy. This does not close the failure. CI now retains the field-level
  failure evidence if it occurs again; native and independent CI retests remain.

Evidence for BUG-042–045, all original failures, source/binary hashes and exact
scope: [functional release fixes](qualification/2026-10-23/functional-release-fixes.json).

## LUMA-BUG-046 — Targeted cancellation re-enters preview visibility

- Status: **fixed; offscreen regression passes, native follow-up open**.
- ADV009's photo switch produced four QML `visible` binding-loop warnings.
  Loading already ended the targeted edit before hiding the Image; its cancelled
  MouseArea then called `endTargeted` again and re-entered `stateChanged`.
- `endTargeted` now ends only a live targeted gesture. The UX scenario rejects
  binding loops; controller coverage checks no redundant notification and no
  splitting of an unrelated gesture's Undo. Three UX repeats and affected
  controller/integration checks pass.

## LUMA-QA-047 — WB/numeric pixel oracle predates Metal rounding budget

- Status: **oracle corrected with explicit limits; affected offscreen retests pass**.
- SLD016/017 failed bit-exact CPU comparison at one RGB step in 48/one channels.
  Actual Metal replay agrees with those retained UI differences and passes the
  pre-existing linear error limit. Six additional extreme WB unit cases pass.
- The UI oracle now applies the existing RGB <= 1, alpha-exact Metal budget
  only to eligible documents with Metal enabled. CPU-only/incompatible paths
  remain exact; negative controls reject two-step RGB/alpha/geometry errors.
  This is an explicit expectation correction; no product pixel math changed.
- SLD015/016/017 pass three repetitions each. New native follow-up remains open.

The same interval passed eight earlier native regressions and three native
PhotoCard repeats; BUG-045 is still open. See [retained follow-up evidence](qualification/2026-10-23/wb-targeted-native-followup.json).

The later three-minute native interval passed SLD016/017 and ADV009 on
Cocoa/Metal DPR 2 without warnings, completing the bounded native follow-up for
BUG-046/QA-047. It also passed the first phase 0.6 Metal preference workflow.
This does not close BUG-045 or full release qualification; see
[the native/cache evidence](qualification/2026-10-23/phase6-native-preview-followup.json).

## LUMA-BUG-048 — Fit to 100% opens the top-left corner

- Reproduced 2026-10-23 by both new P06-ZOM-001/002 adapters on a 3072×2048 chart.
  The initial viewport retained contentX/contentY=0; its visible center was source
  (383,233), instead of (1536,1024). Both original failures are retained.
- Cause: `onPixelModeChanged` centered before the new content-size bindings had
  settled, so Flickable clamped the requested offset to the previous Fit bounds.
- Fix: defer centering with `Qt.callLater` and align the offset to backing pixels.
  Exiting pixel mode still resets Fit offsets and cancels kinetic scrolling.
- Verified locally offscreen: ZOM001/002 pass 3/3 each at even and odd window
  sizes. Actual scene pixels preserve single-pixel detail, and full CPU provider
  pixels, document/revision, selection and original hashes remain unchanged.
  Related PRG002 export passes 3/3, including actual 1024 interaction preview,
  2560 final Fit preview, exact 3072×2048 sRGB PNG and single-gesture Undo/Redo.
- Status: fixed, bounded offscreen and Cocoa/Metal Retina DPR2 confirmation complete.
  The approved native interval passed all three new scenarios on clean `6056dd2`,
  with no QML warnings and intact original hashes/catalogs.
  Full pan/trackpad, external display and phase 0.6 QA remain open.
- Evidence: [zoom/progressive follow-up](qualification/2026-10-23/zoom-progressive-followup.json).

## LUMA-GAP-049 — Viewer zoom modes and input remain incomplete

- Status: **open implementation gap**, confirmed by source audit on 2026-10-23.
- Requirements §16 explicitly requires Fill, 2:1, arbitrary zoom, mouse-wheel
  zoom, pinch zoom and double-click 1:1/Fit. Main.qml currently has only Fit and
  100% controls backed by EditorController's boolean oneToOne, with panning.
  No Viewer wheel/pinch/double-click scale handler exists in the inspected source.
- These behaviors must be implemented and qualified before claiming complete
  Viewer scope. UX-PRV-011/012 and P06-ZOM-003/012 stay unqualified; test inventory
  labels alone do not establish source completion. Existing Fit/100% passes do
  not close this gap. Other Viewer requirements require their own audit.

## LUMA-QA-050 — Preset pixel oracle predates the Metal rounding budget

- Status: **cause reproduced; affected bounded offscreen and native retests PASS**.
- The full functional baseline's PRS017 fails after a valid Light-only paste.
  Three fresh runs retain one differing RGB byte at one level, with exact alpha.
  Captured UI bytes equal fresh actual Metal replay; expected bytes equal fresh
  CPU replay. The unchanged linear budget passes (maximum error 2.38419e-7).
- Presets and WB now share the established RGB <= 1, alpha-exact allowance only
  for Metal-enabled, GPU-compatible documents. CPU-only/incompatible paths remain
  exact. Negative controls additionally reject format, color-space and DPR
  mismatches even when image bytes coincide. Full settings/history/persistence
  checks remain unchanged. Failure pixel capture stays enabled.
- Evidence: [transient preview and preset oracle](qualification/2026-10-23/transient-preview-preset-oracle.json).

The subsequent quiet interval passes PRS017 and SLD016/017 on Cocoa/Metal Retina
DPR2, with intact originals/catalog and no warnings; see
[native follow-up](qualification/2026-10-23/native-preview-zoom-worker.json).

## LUMA-QA-051 — Full CPU test oracle blocks GUI watchdog

- Status: **test oracle moved off GUI; 24 offscreen regressions PASS; hosted retest pending**.
- Hosted bdea5bd CI records 203 PASS / 1 FAIL. ZOM001 repeat 1 is killed after
  over five seconds without a main-loop heartbeat. Its sample contains one LCMS
  frame; passing sibling samples show test `reference -> renderCPU ->
  DisplayTransform` on the GUI thread, with 2678/3573 ms stalls. Source/action
  ordering supports oracle blocking, but the failed caller stack is incomplete.
- Independent zoom references now use `std::launch::async`; ICC is captured on
  the GUI thread and only immutable fixture/settings data reaches the worker.
  Timeout cancels and joins, and failures propagate. Real event-loop heartbeats
  continue; no heartbeat is fabricated and no watchdog or pixel limit changes.
- All eight affected cases pass three repetitions offscreen. The existing
  adversarial watchdog control still kills a fake hung process despite PASS.
  This is a harness correction, not a production rendering performance claim.
- Evidence: [native and worker follow-up](qualification/2026-10-23/native-preview-zoom-worker.json).
