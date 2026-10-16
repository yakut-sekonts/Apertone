# Current-scope UX qualification

Status: **in progress; no release qualification and no production publication**.
Work stays in draft [PR #47](https://github.com/yakut-sekonts/Luma/pull/47),
linked to [Issue #46](https://github.com/yakut-sekonts/Luma/issues/46).
Source scope is 0.5.1; the public stable download remains 0.4.14.

## Verified checkpoint, 2026-10-16

- 266 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, one conditional Depth scenario is
  absent and 177 current scenarios still lack executable adapters. Prepared,
  uncompiled adapters are not counted.
- 16 reproduced production defects have fixes and focused regressions in
  [UX_DEFECTS.md](UX_DEFECTS.md). Raw original failures remain retained.
- The latest navigation/preset/history block ran 19 scenarios three times:
  **57 PASS**, all using the native Metal UI backend. Build provenance, unchanged
  source/binary, original SHA-256 and catalog integrity checks passed.
  Evidence: `navigation-presets-final-report`, run `20261016T145731Z-65579`.
- That block includes eight navigation cases, three logical Undo cases, preset
  hover/Original restoration, idempotence, Natural subset isolation and catalog
  asset isolation. Enter/modal and eight-language checks also passed.
- Full CTest after the unit-fixture normalization: **38 PASS, one optional private
  Nikon SKIP**, 91.02 seconds (`navigation-presets-clean-ctest.log`). The thumbnail
  fixture passed 30 consecutive targeted repeats (`thumbnail-serialized-run.log`).
  A preceding full run's thumbnail timeout and later observer experiment remain
  retained and described in [UX_ENVIRONMENT.md](UX_ENVIRONMENT.md).
- Full build, clang-format and focused clang-tidy passed. Local performance
  remains MEASURE_ONLY; these functional results are not a performance baseline.

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
