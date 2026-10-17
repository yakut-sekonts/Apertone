# Current-scope UX qualification

Status: **in progress; no release qualification and no production publication**.
Work stays in draft [PR #47](https://github.com/yakut-sekonts/Luma/pull/47),
linked to [Issue #46](https://github.com/yakut-sekonts/Luma/issues/46).
Source scope is 0.5.1; the public stable download remains 0.4.14.

## Verified checkpoint, 2026-10-17

- 332 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, conditional mask duplication
  (MSK-005) is absent and 111 current scenarios still lack executable adapters. Prepared,
  uncompiled adapters are not counted.
- 22 reproduced production defects have fixes and focused regressions in
  [UX_DEFECTS.md](UX_DEFECTS.md). Raw original failures remain retained.
- 35 additional adapters cover 500-edit/history and 1,000-gesture stress,
  preset CRUD/round trips/restarts, queued preview races, retained decoded-image
  recovery, partial import failures, 50 cancel/reimport cycles, update/batch
  isolation and current performance measurements. The three new production fixes
  address preset hover ownership, double-tap slider reset and denied import children.
- A further 13 adapters cover WB semantics/restart, 80 numeric-label checks,
  wheel after slider release, missing startup source, fullscreen/scroll retention,
  repeated export Cancel, protected path aliases, immutable queued edits,
  failed autosave/Retry and real scrolling during a 1,000-item export. The new
  formatting fix preserves near-zero edits while removing displayed negative zero.
  Ten cases passed three times in `window-wb-export-final-report`; its three
  numeric fixture failures remain retained. The corrected numeric fixture passed
  three times in `numeric-scroll-settled-report`, and the two native fullscreen
  cases each passed 50 final-source round trips in `fullscreen-fixed-final-report`.
  This resolves **35 Metal results across 13 cases**; provenance, source/binary
  consistency, original SHA and catalog integrity passed for each evidence run.
- Seven more adapters cover deep scroll after WB relayout, photo/language changes,
  actual Tab reachability, held Slider photo/Library transitions and Optics-only
  Copy/Paste/presets. The new fix scrolls focused adjustments into view without
  changing manual scroll or edit history. These and twelve affected existing
  scenarios passed three times: **57 Metal PASS**, `gesture-import-final-report`,
  run `20261017T123506Z-16828`, with provenance, source consistency, original SHA
  and catalog integrity all passing.
- Eleven more adapters cover 255-character filenames, 1,000-character paths,
  large import/export reports, status/layout stability, closed-popup hitboxes,
  disabled actions, actual ToolTips, dynamic scrollbar geometry and localized
  failed-save/close recovery. EXP-011 explicitly reuses the complete VIS-004
  real 100-file failure/readability contract. Manual screenshot review exposed a
  missing sidebar-action oracle; three native failures reproduced the overflow,
  and three more established the intermediate scrollbar/text overlap.
  The complete sidebar now scrolls within clipped bounds and reserves its bar
  gutter, retaining full filename text and real pointer/Tab/Space access to Copy.
  Final evidence: **12 Metal PASS**, `visual-driver-final-report`, run
  `20261017T131309Z-27651`, plus **6 Metal PASS** for VIS-001/DSC-001 three times,
  `sidebar-driver-native-final-report`, run `20261017T132106Z-30385`.
  Nine affected keyboard/preset/layout cases also passed three times before the
  final gutter hardening (`sidebar-focused-final-report`, 27 Metal results).
  Every report passed provenance, source consistency, original SHA and SQLite.
- The hosted auxiliary-window timeout exposed a test-driver ownership error:
  requesting a probe-window update waited on the main-window polish counter.
  Frame acknowledgment now observes the requested window itself. Besides the
  Cocoa repeats, **10 local offscreen/software repeats passed**,
  `driver-frame-offscreen-final-report`, run `20261017T132123Z-27569`.
  This is a local CI-environment simulation, not a successful hosted run.
- The preceding Cocoa block ran 18 scenarios three times: **54 PASS**, all using
  Metal, including actual Cmd+Q, restart, recovery and all three new fixes.
  Build provenance, unchanged source/binary, original SHA-256 and catalog
  integrity passed. Evidence: `cocoa-quit-final-report`, run
  `20261016T222117Z-92749`.
- A separate local CI-environment simulation ran 15 exit/restart scenarios three
  times: **45 offscreen/software PASS** (`offscreen-quit-fixed-report`, run
  `20261016T221827Z-91621`). This is not a hosted GitHub Actions result. The fixture
  now resolves the actual StandardKey.Quit binding; offscreen declares Exit,
  while the Cocoa block separately asserts and exercises Cmd+Q.
- Four isolated idle-host performance scenarios passed in MEASURE_ONLY mode:
  `checkpoint-perf-guard-final-report`, run `20261016T222708Z-94930`. Exact document
  and provider pixels remain required; only scene readback allows one RGBA8 level
  of rounding. These measurements are not performance thresholds or a baseline.
- Full CTest after the latest production fix: **38 PASS, one optional private Nikon
  SKIP**, 83.90 seconds (`visual-driver-full-ctest.log`). All 20 Python runner unit
  tests passed in 5.485 seconds. Full build, clang-format, diff checks and focused
  clang-tidy for VisualRemainingScenarios, UxDriver and BasicScenarios passed.
- Earlier native failures and fixture corrections remain retained and explained
  in [UX_ENVIRONMENT.md](UX_ENVIRONMENT.md). Hosted ca38254 finished with 281
  blocking PASS / one missed import-Cancel input window. The normalized physical
  Copy checkpoint passed nine local offscreen repeats and the native regressions.
  Hosted `3e4fc19` then passed 281 blocking attempts, including those import cases,
  but retained one auxiliary-window polish timeout. The correction passed the
  Cocoa/offscreen repeats above; a new hosted run is required for this checkpoint.
- APP-014 remains unadapted: programmatic QWindow resize bypasses the native user
  minimum constraint, so it cannot prove the specified lower-width acceptance.
  The original failed fixture is retained, without asserting a production defect.

## Remaining qualification

The pending scenarios include long editing/idle/Library sessions, large input
storms, seeded chaos/replay, several storage/import/export races and native OS
input boundaries. In particular, native file-panel opening is covered but its
Cancel input remains unqualified. Trackpad/momentum and unavailable hardware
must be reported honestly, never counted as simulated passes.

This checkpoint is not the full release gate. The Metal UI backend used by Qt
also does not implement the planned 0.6 Metal image-processing backend.
No phase 0.6 production work or release publication starts before the current
scope's qualification and fixes are complete.
