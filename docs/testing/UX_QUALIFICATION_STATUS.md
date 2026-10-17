# Current-scope UX qualification

Status: **in progress; no release qualification and no production publication**.
Work stays in draft [PR #47](https://github.com/yakut-sekonts/Luma/pull/47),
linked to [Issue #46](https://github.com/yakut-sekonts/Luma/issues/46).
Source scope is 0.5.1; the public stable download remains 0.4.14.

## Verified checkpoint, 2026-10-17

- 301 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, one conditional Depth scenario is
  absent and 142 current scenarios still lack executable adapters. Prepared,
  uncompiled adapters are not counted.
- 19 reproduced production defects have fixes and focused regressions in
  [UX_DEFECTS.md](UX_DEFECTS.md). Raw original failures remain retained.
- 35 additional adapters cover 500-edit/history and 1,000-gesture stress,
  preset CRUD/round trips/restarts, queued preview races, retained decoded-image
  recovery, partial import failures, 50 cancel/reimport cycles, update/batch
  isolation and current performance measurements. The three new production fixes
  address preset hover ownership, double-tap slider reset and denied import children.
- The final Cocoa block ran 18 scenarios three times: **54 PASS**, all using
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
- Full CTest after the production fixes: **38 PASS, one optional private Nikon
  SKIP**, 87.13 seconds (`checkpoint-full-ctest.log`). All 20 Python runner unit
  tests passed. Full build, clang-format, diff checks and focused clang-tidy passed.
- Earlier native failures and fixture corrections remain retained and explained
  in [UX_ENVIRONMENT.md](UX_ENVIRONMENT.md). The earlier PR-head CI run timed out;
  a new hosted run is still required after publication of this checkpoint.

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
