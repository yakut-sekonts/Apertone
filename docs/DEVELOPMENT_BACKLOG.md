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

- Status: **open — reproduced during final qualification; blocks release**.
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
