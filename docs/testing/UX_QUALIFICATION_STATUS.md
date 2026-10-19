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

The execution policy classifies 364 compiled native adapters: 252 reviewed bounded
adapters and 112 deferred implemented workloads, plus the independent integrity
adapter. These are separate from the 51 deferred checks among unadapted scenarios.
LIB009 is included in those deferrals because its unchanged REGRESSION contract
requires 50 resize cycles; this deferral adds no adapter or PASS.
The policy preserves inventory modes and marks deferral SKIPPED. New adapters
require workload review. Default CTest uses 256 Library metadata rows; the original
100k contract remains opt-in with `LUMA_ENABLE_SCALE_CONTRACT_TESTS=OFF` by default.

## Verified checkpoint, 2026-10-19

- 364 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, conditional mask duplication
  (MSK-005) is absent. Of the 79 remaining scenarios without executable adapters,
  28 are functional regressions and 51 are deferred load/performance/chaos checks.
  Prepared, uncompiled adapters are not counted.
- 29 production defects are reproduced and fixed with focused native/software
  evidence in [UX_DEFECTS.md](UX_DEFECTS.md). Raw original failures remain retained.
  Three outcomes from a native mask run with confirmed external interaction
  remain individually unattributed. Their FAIL records and subsequent controlled
  diagnostic PASS are retained separately; full release qualification is incomplete.
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

The removable-destination and keyboard checkpoint used source snapshot
`06a50ce703137085eed3e2a7f791dc7fb570574b80150a1d2d5fbec41031f4d9`;
the tested binary SHA-256 is
`76aae6657370eb14b34b12b4353f1551fef8ea8a61d18295c3c55ac87dec5d0b`.
Later documentation-only edits record these outcomes. Full app build, 37 runner
tests, seven policy tests, format and three affected-source tidy checks pass;
pre-existing advisory tidy warnings remain separate. The native lock blocker is
resolved for this cohort, while the 31 functional adapter gaps above remain open.
No tag, production release or phase 0.6 work has started.


## Hosted functional artifact audit, preceding the gradient fix

The downloaded artifact for hosted head `dceb3fbff0ee899ef91d0bd185f1180637fac117`,
run `39226491030`, records **195 specification-domain PASS**, **29 deferred SKIPPED**
and **92 future NOT_IMPLEMENTED**, with no FAIL. The PASS total comprises three
repeats of 64 implemented blocking cases plus three suite-wide original-integrity
results. The 29 deferrals are inventory records, not 87 completed attempts.
A separate supplemental keyboard report records **6 PASS** across Return and
keypad Enter. Both reports use local runner offscreen/software execution on the
hosted macOS machine; neither is a Cocoa/Metal result.

Downloaded archive SHA-256 is
`8137c52ddc94d81c24b327c1da6f80e960d5fb9c7221a3582f63f9d5356e0ac9`.
Build provenance, source consistency and original hashes pass in both reports;
the main report has 192 passing catalog checks, and the keyboard report has six.
GitHub checked out synthetic merge commit
`c5582cf26df9208ea3c3336bf832e9c6af49f0d2`; its attested source tree matches the
clean PR head, SHA-256
`2387256288f548fb6f8df2afcc1dba2e8691969ce76cc5589899a099ca12964b`.
The original archive is retained. The first local audit's incorrect requirement
that merge checkout commit equal PR head was corrected; that audit correction is
not a product fix or a UX result. Both reports retain `release_gate=NOT_QUALIFIED`.
This hosted snapshot predates LUMA-UX-028 and contains no gradient-handle
supplemental result.


## Gradient-handle access checkpoint, 2026-10-19

LUMA-UX-028 is fixed and its bounded functional regression is verified. Existing
Linear and Radial mask geometry remains unchanged by Crop/Done, selection and
handle layout. The local overlay now reserves at least 84×84 logical pixels,
keeps the actual photo rectangle explicit and places all three distinct display
handles inside its input surface. Raw mask coordinates remain unchanged; clipped
raw guides and separate hollow proxy circles do not depict a moved mask. Proxy
press records a frozen source/input anchor, and motion applies relative source
deltas, including rotated radial axes, without an initial snap or a dead area in
thin-photo padding. Ordinary brush, range sampling and empty-canvas redraw retain
their photo and coverage bounds.

Two supplemental cases each use one generated PNG and genuine Crop/Done plus
actual pointer input. Normal and thin Crop variants require all six Linear/Radial
handles to accept real press/drag/release in base, H, HV and V states, with an
independent integer-Crop/source-delta oracle, exact unrelated full-document
fields, one Undo/Redo/Undo, current Ready pixels and natural durable saves. The
thin variant also rejects ordinary Linear redraw, Brush, Luminance and Color
input on an actual empty padding pixel. Original bytes, UUID/path/catalog
membership and complete documents remain required throughout.

Final focused evidence: **6 Cocoa/Metal PASS** in
`mask-edge-verified-final-native-report`, run `20261019T093709Z-53404`, and
**2 local offscreen/software PASS** in
`mask-edge-verified-final-local-software-report`, run `20261019T093545Z-52990`.
Affected official cases have **51 Cocoa/Metal PASS** in
`mask-edge-verified-affected-native-report`, run `20261019T094024Z-55062`,
and **16 local offscreen/software PASS** in
`mask-edge-verified-affected-local-software-report`, run
`20261019T093648Z-53260`. All final child results exit0; build provenance,
unchanged source/binary, original SHA and SQLite integrity pass in every report.
The native affected cohort includes the existing Retina mapping case; the
software results do not qualify that native boundary.

Final application build and CTest pass: **42 PASS and
one optional private Nikon SKIP**, 80.89 seconds,
`mask-edge-verified-full-ctest.log`. Runner accounting has
`40` passing tests and policy accounting has
`seven`; format and the five focused changed-source tidy commands
complete successfully. Advisory diagnostics, including test cleanup and optional-access warnings, remain
recorded separately in the retained tidy logs; none is a command error. The
attested final source tree SHA-256 is `dc3fe1ec0c5189e5223aa20991a94832a6acdbd76d65bc9de514f85577e7d173`; the tested
binary SHA-256 is `7f74fbeab250b70a496ba98cc0a82e0f670d1db6ea5fcaf5ffc617d76901bba5`.

The original one software FAIL and three Cocoa/Metal FAIL remain unchanged in
`msk023-crop-original-local-software-report` and
`msk023-crop-original-native-report`. The first fixed-source thin fixture failure
also remains in `msk023-edge-proxy-first-local-software-report`: its supposed
empty padding point overlapped a legitimate proxy. The corrected negative control
finds a real empty pixel without excluding valid handles, changing production
callbacks or weakening the no-edit/no-sampling/no-extra-history assertions.
The initial CTest readiness failure in `mask-edge-full-ctest.log` is retained
separately from the completed final CTest report.

These two cases run through `run_mask_handle_regression.py` in a separate
supplemental inventory domain. They do not register official UX-MSK-023 or change
the 770-ID specification. At that checkpoint the official adapter count stayed
361 and the functional adapter gaps stayed 31; the later official Auto-completion
adapter is accounted for below.
The bounded plain-Crop/flip matrix does not qualify every zoom, Transform,
projective, pan/recenter or display boundary in full MSK023. Exact CPU reference
pixels use the shared renderer/ICC path and are a UI/state-coherence oracle,
not independent numerical qualification of the renderer. The final local cohorts
are not hosted CI for the new source. No load workload, release tag, production
publication or phase0.6 work is implied by this checkpoint; full current-scope
release qualification remains incomplete.


## Genuine Auto completion checkpoint, 2026-10-19

Official UX-RACE-005 now verifies both genuine successful OLD Auto results:
actual Cancel, and actual filmstrip A→B switch before the original result's
queue/delivery. The test-only worker hold starts after the real solver succeeds;
logical revisions, source UUID/path and retained actual JobData identify the same
result at its original success callback and stale rejection. One delivery and one
rejection are required for each operation. No fabricated callback or solver refusal
can satisfy the successful-result controls. Both stale callbacks are rejected by
the existing first Auto-revision guard; the observed load-revision change does not
independently exercise the later load guard. No production defect or change in
normal Auto behavior is implied by this adapter.

The two imported small assets have independently known complete documents and
nonneutral pixels. Actual controls must finish their seed gestures before Auto;
Cancel preserves the original one-entry Undo/Redo history, while B and the return
to A load fresh histories at their persisted documents. Passive state observations,
current Image Ready/source/provider pixels, natural catalog saves, exact saved
revisions, source SHA and membership remain required. The CPU/ICC reference checks
state coherence through the shared renderer; it does not independently qualify
its numerical implementation or physical displayed-frame pixels. The file-chooser
result is a declared setup seam; native chooser acceptance is not claimed.

Final registered evidence: **27 Cocoa/Metal PASS**, nine bounded cases three times,
`race005-registered-final-native-report`, run `20261019T095737Z-61847`.
That includes three complete UX-RACE-005 attempts and three ordinary successful
Auto Apply/Undo/Redo controls in UX-UND-007, plus manual/guided Transform, Cancel
and photo-switch scroll checks. The same nine cases pass once in local
offscreen/software, `race005-registered-final-local-software-report`, run
`20261019T095722Z-61703`. Every child exits0 and every report passes build proof,
unchanged source/binary, original hashes and catalog integrity. Initial candidate
runs also passed once per backend in separate preserved supplemental reports;
they are not added to the official result totals.

Full build and CTest pass: **42 PASS, one optional private Nikon SKIP**, 81.56s,
`race005-registered-final-full-ctest.log`. Forty runner tests, eight quality-policy
tests, format and four focused tidy commands pass. Advisory tidy diagnostics
remain in the original logs, separate from command errors. The tested source tree
before documentation-only result recording is
`1130e2e1d50029faa2434662817d0dd3cea180a2066cb71e310ea11affda4164`;
the binary SHA-256 is
`3f8781cbe55f545c0c2d5929f9f6a28c9a794deaf38d75e1f239f7466503bf26`.

CI/release workflows execute this REGRESSION case in a separate focused quality
step and retain its own report and operation JSON; the blocking subset alone does
not select it. The one-PNG focused fixture also generates two small DNGs, imports
only two assets and starts Auto twice, with no large fixture or TRN014 cycles.
The mixed local cohort shares ten small PNGs; none becomes a bulk import.
The probe is private to luma_ux_tests and adds no production gate or QObject ABI.

This closes one functional adapter gap: **362 native adapters**, 250 reviewed
bounded adapters and 112 deferred compiled workloads. Of 81 unadapted applicable
scenarios, **30 remain functional gaps** and 51 remain deferred load/performance/
chaos checks; conditional absent MSK005 and independent original-integrity remain
separate. Full MSK023 remains unqualified. The official 770-ID inventory, 28 fixed
production defects, stable0.4.14 and release NOT_QUALIFIED status are unchanged.
No load test, phase0.6 work, tag or production release ran in this checkpoint.


## PhotoCard and real thumbnail-response checkpoint, 2026-10-19

LUMA-UX-029 fixes hardcoded Pick/Reject captions through the existing translation
keys. Its preserved original software failure is followed by a focused software
PASS for all eight actual Settings language choices, with exact independent
literals, current card Image-node/cache pixels, typed documents, metadata, natural
persistence and Undo/Redo. Original and fixed evidence are separate. Native
original was not run; fixed Cocoa/Metal acceptance is pending because the Mac
session remains locked. No complete VIS007/VIS008/LOC qualification is claimed.

A separate supplemental LIB004 candidate observes real QQuickImageResponse
objects and their original lifecycle in the UX target only. Its selected worker
holds a successfully computed OLD image before the original completion queue,
after catalog and provider/cache locks are released. A real scrollbar press must
still own the current visible OLD Loading binding, with no prior reset/cancel/
finish. One real coarse move reveals a previously unrequested NEW tail card;
its genuine request, saved revision, successful result, finished signal, texture
consumption and current Ready pixels must precede explicit OLD release. Actual
processed cancellation is allowed to finish/destroy OLD without a GUI reply.
The same visible UUIDs, viewport, per-card independent pixels and all32 pinned
neutral documents/revisions/metadata/originals must survive that cleanup.

The first software candidate passes in
`card-lib004-fixed-first-lib004-local-software-report`, run
`20261019T103956Z-68969`. Actual grid1079×382, four columns, cell269.75×210 and
cacheBuffer420 require the minimal reviewed32 small RGB PNGs; no large image or
bulk workload is generated. Eight visible cards have own-reference mean RGB
error0 before/after release; the nearest wrong-card error is at least23. The real
OLD response is cancelled by Qt, then emits its genuine finished signal and is
destroyed; no OLD GUI delivery is claimed. This pending-completion boundary does
not establish active disk I/O, exhaustive transient frames or physical scanout.
Official UX-LIB-004 stays unregistered until further acceptance; official counts
remain362 adapters,250 bounded,112 deferred and30 functional gaps.

Mask and Auto diagnostic documents now also retain canonical
`local_masks_serialized`, because preset encoding omits masks. The repeated normal/
thin Crop software cases pass in
`card-lib004-fixed-first-mask-local-software-report`, run
`20261019T103959Z-69021`, and official RACE005 passes in the corresponding
`race-local-software-report`, run `20261019T104102Z-69547`. Typed runtime oracles,
numeric tolerance, full-document/history/pixel/durable checks are unchanged.
This is instrumentation completeness, not a production fix or extra official
adapter. Historical incomplete artifacts remain unchanged.

These first software results share tested source SHA
`151e312372a5471bf20b086420fc2eb57ef855c99ff689c245c2d0ff9fed51c4`
and binary SHA
`6e88661927cf16eeabc9af99e7a822b7ee3868da1ccd53434ae0ac3b24d5d5c9`.
Separate supplemental wrappers preserve both new regressions for CI without
aliasing official IDs. Workload-policy tests reject bulk/duplicate fixture counts
before runner setup and pin exact bounded RGB-only inputs. The full release gate
remains NOT_QUALIFIED; no phase0.6, load test, release tag or production publication.

### Hosted ec51a5d artifact audit

The independently audited archive SHA is
`b0a3893f77e9aa57ca40a092f5af96d950e0d1e51939ac814bc6ef2adf3e1c7e`.
Its four reports contain195 blocking-subset PASS,29 implemented deferred SKIPPED
and92 future NOT_IMPLEMENTED, plus six keyboard, six mask and three RACE005 PASS.
The main195 includes three runner-only SHA observations. There are231 normal
exit0 processes and six expected SIGKILL persistence prepare stages. All normal
raw children report offscreen/software, not Cocoa/Metal. Archived proof agrees
with clean ec51 source SHA
`9fe69d8ffacdd3adb8fba7fc9d3e1234faa33c4a6ca9b958da54dfc6117dcb8f`;
CI checkout is the synthetic merge `f76d4735ba9c88682bbb629410687f2e2cf95179`.
All four release gates remain NOT_QUALIFIED. This archive audit does not establish
completion of remaining workflow steps or independently recheck SQLite/original
bytes, which are absent. The historical mask JSON contains all144 typed-check
markers but omits numerical local-mask deltas; those cannot be reconstructed from
that archive. Historical runtime PASS is retained with this explicit evidence limit.


## Native acceptance and interrupted-run evidence, 2026-10-19

After the Mac was observed unlocked, final PhotoCard captions and real thumbnail
responses each passed three times on Cocoa/metal DPR2 and three times in local
offscreen/software. These are twelve supplemental results, not twelve new
specification adapters. Reports are `card-thumbnail-retina-card-native-report`
(`20261019T105445Z-73292`), `card-thumbnail-retina-card-software-report`
(`20261019T105433Z-73165`), `card-thumbnail-retina-lib004-native-report`
(`20261019T105417Z-73045`) and `card-thumbnail-retina-lib004-software-report`
(`20261019T105410Z-72993`). Their exact tested source is
`5a1a35eac6846e715c67f77bf2b8d1e8a49ca214b9ed9611448336d67624fd69`;
binary is `c4613c871f61c148d5fb7da0ed805bbb449518fd8287814cb8c353e0259935d4`.
All proof, original SHA and catalog checks pass. Card captions now have focused
native acceptance; full VIS007/VIS008/LOC remains outside that result.

Two fixture corrections retain their original failures. The caption fixture's
single wheel event failed to move the clipped grid in three software attempts;
its cause is unexplained. One real scrollbar drag now reveals the same strict
caption bounds with acquired press, directional displacement and legal settled
offset; no setter/retry or translation-oracle change is involved. The first three
native thumbnail attempts failed before the gate because Qt requests768×768 on
Retina for the declared384×384 source. The test now independently predicts
`QSize(384,384) * window.effectiveDevicePixelRatio()` before arming, verifies the
logical sourceSize separately, and requires that exact physical size on both
actual OLD/NEW tuples. No response wildcard or production size change is used.
The retained native failure trace proves the size mismatch; corrected DPR1/DPR2
runs pass with genuine current OLD Loading and real terminal cleanup.

The same source's expanded mask regression produced **three PASS and three FAIL**
in `card-thumbnail-retina-mask-native-report`, run `20261019T105606Z-73698`.
Normal repeat2 timed out on the existing conjunction localMode/localCanvasReady/
not-localSampling after a final Undo; the full typed document assertion had
already passed. Thin repeats1/2 timed out on window polish and evidence capture.
The Mac was unlocked. These are **unresolved native outcomes**, not proven render
races, fixture failures or extra production defects. The old messages do not
separate the predicate terms, current error, window exposure or other input.
Original reports, actions, screenshots and mixed counts remain unchanged.

A passive bounded readiness timeline now records distinct public controller and
window states on genuine stateChanged/afterAnimating, with a final document before
Context cleanup. It makes no render, cancellation, input, wait or acceptance
change. Both software cases pass in `mask-ready-passive-mask-software-report`,
run `20261019T110201Z-75351`; all six native attempts pass in
`mask-ready-passive-mask-native-report`, run `20261019T110304Z-75610`.
The six native timelines have no dropped/failed samples, active exposed windows,
no preset preview and an unchanged physical cursor. This does **not** explain or
fix the three prior failures: observation can affect scheduling, and the old
outcomes lack those fields. The user subsequently confirmed using the Mac during
the first mask run. External interaction could affect native focus/input, but the
individual causes are not established. Those original outcomes are retained as
an externally interrupted run; they neither prove a production defect nor become
PASS. The later six passing native timelines independently establish their own
bounded functional acceptance, without a further production fix.

Canonical mask artifacts now independently expose all192 accepted geometry edits
and exact restored documents across those eight attempts, including base/H/HV/V
for both shapes. Saved actual/expected numerical differences are at most
1.6653345369377348e-16, within the unchanged less-than1e-12 runtime tolerance.
This repairs future artifact completeness; the old CI mask omission remains an
explicit historical limit. Full official MSK023 stays unqualified.

Four ordinary no-Session provider controls (REC009 clean corruption/restart,
SEL002 card activation, SEL017 filmstrip switch and LOC005 Unicode flow), plus
RACE005, pass five times once in software and fifteen times across three native
repeats. See `mask-ready-passive-provider-control-software-report` and
`mask-ready-passive-provider-control-native-report`. REC009 has an independent
Ready/cache oracle; the activation/filename cases are not relabelled thumbnail
race or pixel qualification. Auto still requires its genuine solver completions.

Full build/CTest pass:42 PASS plus one optional private Nikon SKIP,82.26s, in
`mask-ready-passive-full-ctest.log`. That includes40 runner and10 workload-policy
unit tests. Format and all seven focused tidy commands exit0; advisory diagnostics
remain in logs. This last tested source before documentation-only recording is
`cedef5a1ab8e28090de5b091f928a31cd5d9129098705a25c3704caff0c00e03`;
binary is `8af6e5692443bc7452a3c78d6d6206288ac298f430035919b7dc272c3135b18d`.
The private trace define is absent from both ordinary production/thumbnail-unit
provider compile commands and present only in the UX target.

GitHub subsequently confirms the earlier clean ec51a5d workflow run39234353511
completed successfully, including clang-tidy. Its uploaded evidence audit and
historical mask limitations remain as recorded above. This is not CI completion
for the newly changed source. Official counts remain362/250/112 and30 functional
gaps. LIB004 remains a supplemental candidate pending generic fixture/inventory
integration; a prepared RACE002 draft has no runtime result and is not counted.
No load test, phase0.6 work, release tag, merge or production publication occurred.
Full current-scope release qualification is still NOT_QUALIFIED.

## Official thumbnail response regressions and complete bounded pass

The next checkpoint registers official `UX-LIB-004` and `UX-RACE-002`, keeping
their existing specification modes, scopes and severities. There are now 364
compiled native adapters: 252 reviewed bounded and 112 deferred implemented
workloads, plus the global integrity adapter. Of 445 applicable scenarios,
conditional MSK005 is absent and 79 remain unadapted: 28 functional and 51 deferred
load/performance/chaos checks. No production defect is added to the 29 reproduced
fixes by this checkpoint.

LIB004 uses exactly 32 fresh opaque RGB sources; RACE002 uses two. Separate
run-owned case directories preserve ordinary fixture counts and bytes for every
other selected case. Their path-based SHA union is checked after every case and
at suite end. Equal contents at different owned paths remain distinct originals.
The manifest records the actual base/extra/total counts. Alias, duplicate-path,
symlink, digest, missing-file, mutation and ineligible-selection tests fail closed;
an explicitly ineligible profile-only request never falls back to 1000/Large.
All 67 combined Python infrastructure tests pass, including 17 dedicated fixture
tests, 40 runner tests and 10 workload-policy tests.

RACE002 observes a genuine successful OLD request held before its original
completion queue. A real Exposure gesture naturally saves revision 1 and emits
one targeted A invalidation. The real NEW response must read that revision and
reach Ready with its independent saved-document pixels while OLD is still held.
Explicit OLD release, finished and destruction preserve current NEW pixels and
the untouched B asset. Separate actual Undo/Redo produce revisions2/3. Full typed
documents, metadata, row fields, session/import journal, source SHA and history
are pinned independently. This is request/completion ordering and UI/data
coherence, not an independent numerical renderer or physical scanout proof.

The first candidate attempt, `lib004-official-race002-first-race002-software-report`,
run `20261019T112549Z-79939`, crashed in Driver::type when the declared FileDialog
selection seam left no active text focus. Its crash report is retained. The next
attempt, `race002-dialog-focus-software-report`, honestly timed out waiting for
focus return. Both failed before the race boundary; neither proves a production
race defect. Setup now explicitly reactivates its own window before installing
the Session, then requires actual focus. Native picker selection and platform
focus return remain unqualified. Driver::type checks null before sendEvent.
The unchanged race contract then passed three times per backend as a candidate;
those original supplemental results are not relabelled official results.

Official final reports use prefix `official-thumbnail-final`: six software and
six Cocoa/Metal results across LIB004/RACE002, nine mixed results with SEL002,
IMP009 and UND018, eight affected controller/input results and two supplemental
card-caption results. All 31 rows PASS, with one global-integrity row included.
Mixed SHA unions are 34/46/56 as planned. Independent trace/pixel/SQLite/SHA audit
found no discrepancy across all 18 thumbnail executions. Full build, format and
four focused tidy commands exit 0; advisory diagnostics remain in their logs.
CTest passes 44 tests plus one optional private Nikon SKIP, 45 total, 84.50s.

The broader local offscreen/software run, `all-current-quality-281a591f-report`,
run `20261019T114156Z-86851`, selects all 444 current rows except native-only
MAC001. It passes 251 real application scenarios plus the global SHA check:
**252 PASS, 191 SKIPPED, one NOT_IMPLEMENTED, zero FAIL**, 398.638s. SKIPPED comprises
112 implemented deferred workloads, 51 unadapted deferred checks and 28 functional
gaps; MSK005 is the single absent conditional capability. Its raw exit 1,
execution INCOMPLETE and release NOT_QUALIFIED are expected and preserved. It
creates only 20 base PNGs, two small DNGs and 34 dedicated RGB sources, 56 originals
in total, with no 24MP/large-catalog fixture and no deferred execution.

MAC001 separately passes its strict native AppKit/Metal/Retina contract in
`official-thumbnail-final-retina-native-report`. Thus all 252 currently implemented
bounded native adapters have execution evidence for this source: 251 in software
and one native-only Retina case. This does not close the 28 unadapted functional
scenarios or qualify deferred workloads. The final official, broad and Retina
cohorts attest source
`281a591fe832665bdfe87ef8d1094324ebab206c452b0c60ae16280ba917680c`
and executable `67a284b38a337c174d2be0a7a460aa9f71f6fee638ac0d2f786af2bfd21429e5`.
Provenance, source consistency, original SHA and catalog integrity pass. Local
GITHUB_ACTIONS simulation is not hosted CI.

The earlier 50f6e16 hosted run 39242877166 has completed its functional/supplemental,
artifact-upload and format steps successfully; clang-tidy was still in progress
at this recording. Its downloaded 47,360,440-byte ZIP matches API SHA
`722646f2a5c5661465b7c1cfe4be6c00e9fc0146ca9c3b0897913571e94d92e3`.
Actual checkout051586954bbb2bdcc5319baa8468ef53b32c48f8 is the verified synthetic
PR merge, whose second parent is50f6e16 and whose Git tree equals that head tree.
That archive is evidence for the prior source7085381e checkpoint, not current281a.
No workflow completion, release, tag, main merge or phase0.6 work is claimed here.
