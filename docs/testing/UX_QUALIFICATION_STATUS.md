# Current-scope UX qualification

Status: **in progress; no release qualification and no production publication**.
Work stays in draft [PR #47](https://github.com/yakut-sekonts/Luma/pull/47),
linked to [Issue #46](https://github.com/yakut-sekonts/Luma/issues/46).
Source scope is 0.5.1; the public stable download remains 0.4.14.

User direction, 2026-10-19: run functional quality checks only. Stress, soak,
chaos and performance workloads are reserved for the pre-release campaign of 1.0.0.
Historical load-test evidence stays historical; deferred work does not count as PASS.
Current runs use explicit functional cases, excluding large-catalog and repeated
stress fixtures even when their inventory mode is REGRESSION.

The execution policy classifies 361 compiled native adapters: 249 reviewed bounded
adapters and 112 deferred implemented workloads, plus the independent integrity
adapter. These are separate from the 51 deferred checks among unadapted scenarios.
LIB009 is included in those deferrals because its unchanged REGRESSION contract
requires 50 resize cycles; this deferral adds no adapter or PASS.
The policy preserves inventory modes and marks deferral SKIPPED. New adapters
require workload review. Default CTest uses 256 Library metadata rows; the original
100k contract remains opt-in with `LUMA_ENABLE_SCALE_CONTRACT_TESTS=OFF` by default.

## Verified checkpoint, 2026-10-19

- 361 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, conditional mask duplication
  (MSK-005) is absent. Of the 82 remaining scenarios without executable adapters,
  31 are functional regressions and 51 are deferred load/performance/chaos checks.
  Prepared, uncompiled adapters are not counted.
- 27 reproduced production defects have fixes and focused regressions in
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
- Six further Library adapters cover 4↔5-column reflow around the same logical
  asset, filter changes during an actual drag, cleared/empty result ranges,
  ungated physical import progress during scrolling and held Select all completion
  overlapping durable insertion. The import/reload regression reproduced a real
  viewport jump three times. The fix resolves the same visible asset in the new
  filtered/sorted result on the metadata worker and restores its bounded offset,
  while filters, superseded reloads and user input invalidate old restoration.
  Final evidence: **18 Metal PASS**, `library-viewport-final-report`, run
  `20261017T174218Z-45300`, plus **39 affected Metal PASS**,
  `library-viewport-affected-report`, run `20261017T174723Z-45228`.
  Each import/scroll repeat executes separate 2,048-source insertion waves before
  and after the old assets, then an actual import reload while a scrollbar is
  held. All three observed that real held input suppresses restoration and that
  post-release scrolling works. Provenance, source consistency, original SHA and
  SQLite integrity passed for every result.
- Eight further adapters cover repeated batch Cancel, editor/batch history
  arbitration, 100 full Apply/Undo cycles over 100 assets, idle update popup,
  update-check failure, notification during a held Slider and simultaneous
  save/export close with overlapping genuine closeReady delivery. Final evidence:
  **24 Metal PASS**, `batch-update-final-report`, run `20261017T180206Z-54876`,
  plus **30 affected Metal PASS**, `batch-update-affected-report`, run
  `20261017T180407Z-56461`. All provenance, source consistency, original SHA and
  catalog-integrity checks passed. Batch growth assertions cover permanent
  row/page footprint for the fixed workload, not allocator RSS or worker TEMP tables.
  UPD-005/006 replace only the NativeUpdater SDK interface, deliver its actual
  callbacks to the real UpdateController, and record that Sparkle network,
  appcast, package download and install were not exercised. Those two cases also
  passed three times in a **local offscreen/software simulation**,
  `batch-update-offscreen-report`, run `20261017T180522Z-57220` (six PASS).
- Three more adapters cover immutable image-watermark overwrite/deletion after
  enqueue, complete named-profile versus last-used settings across two real
  processes, and all four metadata privacy policies on actual PNG outputs.
  Primary ImageIO readers independently check literal GPS/IPTC/EXIF/attribution
  tags, both metadata/properties channels and forbidden serialized XMP values.
  Full settings and profile UUID are checked against independent literals;
  watermark geometry/pixels and all source/document identities remain exact.
  Final evidence: **9 Metal PASS**, `export-settings-unlocked-final-report`, run
  `20261017T222930Z-65253`, plus **18 affected Metal PASS**,
  `export-settings-affected-report`, run `20261017T223110Z-65856`. The same cohorts
  passed in a local offscreen/software simulation (9 + 18 PASS). All provenance,
  source consistency, original SHA and SQLite checks passed. Full application
  build, runner-accounting CTest (25 tests, 5.66 seconds), format and four focused
  clang-tidy checks passed. Nine preceding native activation failures were
  caused by a locked Mac session and remain retained in the original report;
  the user unlocked the Mac before the successful new report was recorded.
- Two more adapters execute 500 byte-distinct private inputs with a persisted
  seeded pause/resume schedule and 100 actual Start/Cancel cycles. Final evidence:
  **6 Metal PASS**, `export-stress-final-report`, run `20261017T225237Z-70937`,
  plus **18 affected Metal PASS**, `export-stress-affected-report`, run
  `20261017T225511Z-71714`. A local offscreen/software simulation passed both
  complete workloads once. Provenance, source consistency, original SHA and
  SQLite integrity passed throughout. Cancel overlaps a genuinely installed and
  committed first output while its original completion remains held; committed
  files survive and unstarted files and temporary artifacts remain absent.
  This does not qualify cancellation inside an encoder or restart persistence.
  Full build, 25 runner-accounting tests, format and two focused tidy checks passed.
- Three more adapters execute all 200 Export open/close cycles, all 200 Import
  open/Cancel cycles and 200 real Detail keyboard edits followed immediately by
  Export. Independent full documents cover every Undo and Redo step. A genuine
  held final catalog write proves Export waits for the last natural autosave;
  exact PNG pixels, three originals and durable import/catalog state remain
  correct. Final evidence: **9 Metal PASS**, `panel-detail-final-report`, run
  `20261018T072955Z-79020`, plus **21 affected Metal PASS**,
  `panel-detail-affected-report`, run `20261018T073124Z-79528`. All provenance,
  source consistency, original SHA and SQLite checks passed. The complete three
  workloads also passed once in a local offscreen/software simulation. Full
  application build, 25 runner tests (5.67 seconds), format and focused tidy passed.
  RSS/object counts are raw observations without a leak or plateau assertion;
  QML Import Cancel does not qualify native file-panel Cancel.
- Three more adapters cover the actual two-retry thumbnail cap and recovery,
  saved thumbnail invalidation while the asset is offscreen, and native Retina
  2x pointer/render mapping. Exact JPEG cache pixels, all visible Image-node
  readbacks, original hashes and complete durable documents remain required.
  Retina uses independent AppKit corner conversions, actual Metal frame pixels,
  real Slider/mask/Crop inputs, rendered handle centroids and exact history.
  Final evidence: **9 Metal PASS**, `thumbnail-retina-final-report`, run
  `20261018T074529Z-82693`, plus **21 affected Metal PASS**,
  `thumbnail-retina-affected-report`, run `20261018T074623Z-82997`.
  The two Library workloads also passed once in local offscreen/software;
  Retina has no software/forced-scale fallback. All provenance, source stability,
  original SHA and SQLite checks passed. Full build, 25 runner tests (5.65
  seconds), format and focused tidy passed. Initial fixture failures remain
  retained; no production behavior was changed by this block.
- One scoped seeded history-chaos adapter executes all 10,000 semantic actions
  through three real Basic Sliders and Undo/Redo. Independent full typed-document
  hashes and a capped timeline model check every action; natural durable saves,
  exact Ready pixels and SQLite integrity are checked every 100 actions and at
  completion. The default seed, an immutable replay in a new process and seed0
  all passed on Cocoa/Metal: **30,000 native actions**, in
  `history-chaos-first-native-report`, `history-chaos-replay-report` and
  `history-chaos-seed-zero-report`. Replay bytes and full checkpoint measurements
  were identical. Another **10,000 actions** passed in a local offscreen/software
  simulation; **18 affected native attempts** passed in
  `history-chaos-main-affected-report`, run `20261018T082544Z-92058`.
  Every run passed provenance, source stability, original SHA and SQLite checks.
  A structurally valid replay with another app version was rejected by the actual
  native binary before Context/window/catalog/seed creation; its raw expected
  rejection remains distinct from UX PASS. The broad Chaos Monkey action pool,
  other CHAOS IDs and randomized worker delays remain outside this adapter.
- One further export adapter covers the supported mutually exclusive resize
  modes and retained scalar: seven modes on landscape/portrait inputs produce
  14 actual PNGs per attempt. Independent literal dimensions and direct 2D
  area/bilinear resampling check the complete output, allowing one RGBA8 level
  for float accumulation; CPU development and ICC encoding are shared and not
  independently qualified. Full document/history, catalog metadata/journal,
  immutable originals and prior outputs remain protected. Final evidence:
  **3 Metal PASS**, `resize-shutdown-final-native-report`, run
  `20261018T191631Z-7773`, plus **18 affected Metal PASS**,
  `resize-shutdown-affected-native-report`, run `20261018T191745Z-8438`.
  A local offscreen/software cohort passed the same 14-output case three times
  and nine genuine SIGKILL/restart cases (12 PASS). All proof, source stability,
  original SHA and SQLite checks passed. Four infrastructure CTests passed
  (8.38 seconds), including 30 runner tests; format and focused tidy passed.
  Test-only shutdown markers preserve natural owner destruction and observed
  zero wrapped SQLite files in all 39 normally exiting child snapshots. This
  is an observation, not a worker-drain barrier or a root-cause verdict for the
  earlier hosted abnormal exit. No production behavior changed in this block.
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
- Latest full CTest: **40 PASS, one optional private Nikon SKIP**, 87.51 seconds
  (`history-chaos-full-ctest.log`). This includes the independent history model/
  replay-plan target, 26 Python runner tests and five replay-routing tests.
  Full application build, format/diff and focused clang-tidy for HistoryChaosPlan,
  HistoryChaosScenarios, HistoryChaosModelTests and main passed. The preceding
  production viewport correction separately passed full CTest (86.59 seconds),
  including new Catalog/filter/rank and superseded-model-reload unit cases.
- Earlier native failures and fixture corrections remain retained and explained
  in [UX_ENVIRONMENT.md](UX_ENVIRONMENT.md). Hosted ca38254 finished with 281
  blocking PASS / one missed import-Cancel input window. The normalized physical
  Copy checkpoint passed nine local offscreen repeats and the native regressions.
  Hosted `3e4fc19` then passed 281 blocking attempts, including those import cases,
  but retained one auxiliary-window polish timeout. The correction passed the
  Cocoa/offscreen repeats above. Hosted `56e4e28` then completed with **282 blocking
  PASS**, passing build, unit/integration, format and clang-tidy. Its source
  consistency and original SHA passed; its build-provenance file was absent,
  so those functional results do not qualify a formally attested release build.
  CI/release workflows now build through the attestation wrapper, require matching
  source/binary hashes before UX execution and retain the proof in their artifacts.
  All 25 runner unit tests passed (5.468 seconds), including 22 mocked CLI matrix
  cases for strict/ad hoc modes and source/binary drift. A real stale-build
  preflight rejected execution with exit 2 before fixture/child execution.
  Hosted `3183ff9` then passed **282 blocking attempts** and all build/unit/static
  steps (run `39056014400`). Its uploaded source/binary proof, source consistency,
  original SHA and 279 native catalog-integrity results all passed. Its source hash is
  identical to the local clean-commit snapshot. This confirms the CI attestation
  path; the blocking subset is still not full current-scope release qualification.
- Earlier hosted `29074c6` run `39110825283` passed build, unit/integration and
  attestation but failed the blocking subset: **281 PASS, one SLD-020 FAIL**,
  with 92 future NOT_IMPLEMENTED. Only its first verify process terminated
  abnormally after the visible quit path; repeats2/3 passed. The original runner
  omitted the exit code and retained an empty message, so a product root cause
  is not yet established. Raw artifact and its SHA are preserved; supervisor
  diagnostics are strengthened before targeted restart reproduction. This CI
  failure remains unresolved and cannot qualify a release.
- Latest hosted `5af765f` run `39156735777` completed all steps successfully:
  build, unit/integration, attestation, **282 blocking PASS**, format and tidy.
  Downloaded artifact SHA, source/binary proof, original SHA and 279 reported
  catalog-integrity results passed. All three SLD-020 prepare/verify pairs exited0;
  this does not retrospectively establish the cause of the earlier failed run.
- APP-014 remains unadapted: programmatic QWindow resize bypasses the native user
  minimum constraint, so it cannot prove the specified lower-width acceptance.
  The original failed fixture is retained, without asserting a production defect.

- MAC-007 now covers real owned disk-image source detach/remount after actual
  Add import. Full UUID/document/revision/metadata/journal/source SHA, actual
  missing-source read error, retained control session and exact main/thumbnail
  pixels survive recovery. This is a two-source functional fixture, with no
  physical USB claim. It reproduced LUMA-UX-024: first-page requests were refused
  synchronously during populated modelReset. Publishing readiness before that
  signal fixes the blank cards; a one-shot startup/refresh/filter unit fails on
  the original ordering and passes after the fix. Full build and **40 CTest PASS,
  one optional Nikon SKIP** passed (101.67 seconds). Three complete native
  candidates passed in `volume-source-qualified-candidate-native-report`;
  previous blank-card, thumbnail-fixture and two unexplained polish failures
  remain preserved and separately described in UX_ENVIRONMENT.md. The registered
  final source passed **21 functional Cocoa/Metal attempts** (MAC-007 and six
  affected cases, three repeats), `volume-source-final-native-report`,
  `20261019T071449Z-17632`, plus **3 local software PASS**,
  `volume-source-final-software-report`, `20261019T071619Z-18703`.
  All 27 actual child stages exited0 with raw PASS; provenance, source stability,
  original SHA and catalog integrity passed. Each of four final own-volume runs
  accepted two attachment generations and checked final ordinary detach.
  Format, 30 runner-accounting tests and focused changed-source tidy passed;
  baseline advisory warnings remain separate. No load workloads ran in this
  final cohort.

## Remaining qualification

Pending functional quality checks include storage/import/export races and native
OS/input boundaries. Long editing/idle/Library sessions, large input storms,
seeded chaos/replay and performance workloads are reserved for the pre-1.0.0
campaign. Native file-panel opening is covered but its
Cancel input remains unqualified. Trackpad/momentum and unavailable hardware
must be reported honestly, never counted as simulated passes.

This checkpoint is not the full release gate. The Metal UI backend used by Qt
also does not implement the planned 0.6 Metal image-processing backend.
No phase 0.6 production work or release publication starts before the current
scope's requested functional quality qualification and fixes are complete.


## Removable destination and keyboard checkpoint, 2026-10-19

MAC008 requires both Copy and Export in fresh processes/catalogs for every repeat;
a failed Copy prevents Export and a failed Export cannot inherit Copy PASS. Each
branch has three preceding native candidate PASS, real owned-image detach/remount,
source/document/catalog identity and checked final detach. The official aggregate
also passes in the focused local software cohort. This covers ordinary loss at the
established file/host-WAL boundaries; it does not claim physical USB or mid-encoder
loss. The new single-link unit fails the original Copy and passes the fix.

The selected-photo keyboard regression has a separate supplemental domain and a
single generated PNG. Separate fresh processes cover Return and keypad Enter;
each requires one exact UUID/open/load with zero acceptance pointer events. Native
chooser, OS repeat cadence and full KEY015 remain unqualified. CI runs these two
supplemental adapters separately from the 770-ID specification.

Complete build and CTest pass: 42 PASS and one optional private Nikon SKIP (97.32s),
`quality-fixed-final-ctest.log`. Runner accounting passes 37 tests and
quality-policy accounting passes seven. Focused local software covers seven official
cases plus the supplemental keyboard case. Source consistency, attested build,
original SHA and SQLite checks pass. Latest preceding hosted commit `7c2c59c`
completed build, CTest, its blocking UX subset, format and tidy successfully
(run `39215333610`); that run predates this checkpoint's new changes.

The original full reviewed software cohort has 248 PASS and one DSC006 FAIL; passive
traces reproduce its layout clamp three times. The fixed-source full software rerun
(`quality-reviewed-fixed-local-software-report`, `20261019T083331Z-35766`) has
248 PASS, including DSC006, and one DSC003 FAIL. These are local offscreen/software
runs with matching source/binary proof, original SHA and 248 catalog checks each;
the global original-integrity adapter has no catalog. They are not hosted CI or
native Metal qualification.

DSC003 diagnostics preserve two further original FAIL and one PASS in
`dsc003-original-oracle-confirm-local-software-report`, `20261019T084551Z-38913`.
Both failures observe y=0 after one animation tick, then natural y=33 about 15 ms
later without another input, focus change, document change or layout change.
This proves an early test oracle, not a production wheel-routing defect. The
fixture now acknowledges actual displacement from that same single wheel within
one second, then checks settled positive in-range offset, unchanged geometry and
exact full document. DSC003, DSC006 and two related Slider cases pass three times
in `dsc003-acknowledged-final-local-software-report` (12 PASS). The earlier full
report retains its FAIL; the focused result resolves that case without rewriting it.

The earlier native rerun has one PASS and 20 FAIL while the Mac becomes locked
(`mac008-quality-final-native-report`, `20261019T080838Z-30627`). Its original OS
lock evidence and raw failures remain retained. A later read-only OS probe confirms
IOConsoleLocked=false, permitting a fresh native run without changing Mac security
settings. The final affected cohort passes **30 Cocoa/Metal results** across ten
bounded scenarios, including native Retina, Copy/Export remount, import rollback,
selection/keyboard and DSC003/006: `quality-checkpoint-final-native-report`,
`20261019T085424Z-41974`. Both supplemental keyboard routes pass three times
(**6 Metal PASS**), `keyboard-two-routes-final-native-report`,
`20261019T085602Z-43302`. The same final code passes 27 official and six supplemental
local offscreen/software results; those are not hosted CI. Every final report
passes build provenance, unchanged source/binary, original SHA and SQLite checks.

The final source snapshot is
`06a50ce703137085eed3e2a7f791dc7fb570574b80150a1d2d5fbec41031f4d9`;
the tested binary SHA-256 is
`76aae6657370eb14b34b12b4353f1551fef8ea8a61d18295c3c55ac87dec5d0b`.
Later documentation-only edits record these outcomes. Full app build, 37 runner
tests, seven policy tests, format and three affected-source tidy checks pass;
pre-existing advisory tidy warnings remain separate. The native lock blocker is
resolved for this cohort, while the 31 functional adapter gaps above remain open.
No tag, production release or phase 0.6 work has started.
