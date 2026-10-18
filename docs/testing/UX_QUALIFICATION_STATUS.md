# Current-scope UX qualification

Status: **in progress; no release qualification and no production publication**.
Work stays in draft [PR #47](https://github.com/yakut-sekonts/Luma/pull/47),
linked to [Issue #46](https://github.com/yakut-sekonts/Luma/issues/46).
Source scope is 0.5.1; the public stable download remains 0.4.14.

## Verified checkpoint, 2026-10-18

- 358 native scenario adapters and one suite-wide original-integrity adapter.
  Of 445 scenarios through the current phase, conditional mask duplication
  (MSK-005) is absent and 85 current scenarios still lack executable adapters. Prepared,
  uncompiled adapters are not counted.
- 23 reproduced production defects have fixes and focused regressions in
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
  original SHA and 279 native catalog snapshots all passed. Its source hash is
  identical to the local clean-commit snapshot. This confirms the CI attestation
  path; the blocking subset is still not full current-scope release qualification.
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
