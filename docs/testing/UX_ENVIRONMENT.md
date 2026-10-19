# Qualification findings outside production controller logic

These findings keep their original FAIL evidence. A later passing repeat does
not retroactively remove a watchdog event or qualify the full current scope.

## macOS clipboard waits during Qt startup / activation

The blocking run `20261016T121010Z-22497` recorded two watchdog failures in
`UX-MSK-022/repeat-1..2`, before any mask input was sent. Original hashes and
catalog integrity remained intact.

- Repeat 1: the main thread was in Context's initial activation wait, then
  `QQuickTextInput::q_canPasteChanged` → `QMimeData::hasText` →
  `CFPasteboardGetItemCount` → synchronous XPC wait.
- Repeat 2: the main thread was still in `QGuiApplication` construction, before
  Context or QML existed: Cocoa platform integration → `PasteboardCreate` →
  `CFPasteboardCreate` → synchronous XPC wait.

Both samples and raw failures are retained under the run's scenario directories.
The stacks establish where the threads waited; they do not prove the underlying
cause in Qt, macOS or another clipboard participant. No clipboard content was
read for diagnostics, no clipboard data cleared and no system service restarted.
The following isolated three mask repeats passed, so startup/activation
instability remains unresolved rather than being labeled a mask correction fix.

## Comparison fixture previously accepted an intermediate render

`UX-PRV-001/repeat-3` in the same blocking run timed out on its first Processed
comparison. The old fixture accepted any preview different from the unedited
image immediately after a multi-event slider drag. That condition did not prove
that the captured frame represented the final edit document.

The fixture now computes reference pixels from the final document and waits for
the matching QML Image URL, Ready status and provider pixels before comparison.
It retains intermediate/final images when the earlier capture differs. Numerical
renderer validation is separate; this oracle checks UI/document coherence.
The original failure remains available. Original/Processed each passed three
strict repeats after this change (`pixel-clipboard-locale-report`), without a
production comparison change in this round.

## Graphics backends

Local native suites now use Qt's default graphics backend, matching production;
each result records the actual graphics API. On this reference Mac it is Metal.
Hosted CI continues to use offscreen/software. Earlier local functional reports
used software; they remain evidence for that backend, without implying Metal
coverage. PERF_LOCAL already used the default backend before this change.

## Test fixture corrections: keyboard focus and collection names

`keyboard-extra-report` initially assumed that a mouse click transfers keyboard
focus to every button/slider. macOS does not guarantee this. The corrected Tab
tests first reach their starting control through actual keyboard navigation;
focus order, ScrollView escape and popup focus restoration then passed three
times without production changes for those behaviors. Enter activation was a
separate reproducible product/specification gap, tracked as LUMA-UX-013.

`filter-keyboard-native-report` initially expected names differing only in case
to be rejected. The existing catalog schema uses a case-sensitive UNIQUE name
constraint; such names are distinct. The corrected test checks duplicate exact
names, empty/whitespace-only errors, deterministic repeated failures, distinct
case variants and successful retry. No collection-name policy change was made.

## Gradient fixture sent input before the post-Redo canvas was ready

The full Metal blocking run `20261016T135909Z-46869` retained two failures of
MSK-021, with unchanged gradient geometry after the second drag. Instrumentation
preserved the same input sequence and reproduced two failures in ten attempts
(`gradient-press-stress-report`, run `20261016T141532Z-53585`). Both failures
recorded local_canvas_ready=false and gesture_active=false immediately after
press; all eight successes recorded both flags true. Redo invalidates the canvas
until the current render completes, and beginLocalGesture deliberately refuses
input before that point. The fixture omitted its existing ready condition here.

The corrected fixture waits on localCanvasReady after Redo and verifies that the
handle press starts a gesture. No fixed delay or production mask change is used.
Raw original failures, input traces and screenshots remain retained.

## CI evidence paths ignored a custom build directory

The initial CLI used build-verify for default reports/artifacts even when invoked
with --build build, while both workflows upload build/ux-*. A mocked CLI reproducer
confirmed the mismatch without launching Qt. Defaults now follow the selected
build directory; explicit paths are preserved. The runner unit suite tests both
CLI variants so CI uploads the evidence it actually generated.
The lock file is scoped by a context manager and closed on return or exception;
the CLI test also checks that it does not retain screen ownership after returning.

### Native file-panel cancel input remains unqualified

`navigation-first-report`, `navigation-repro-report` and
`native-cancel-input-report` retain unsuccessful harness experiments for NAV-004.
The panel is an own-process NSOpenPanel with NSRemoteView as first responder;
synthetic NSEvent dispatch did not dismiss it. Its public accessibilityCancelButton
was unavailable. This is not evidence that a real Cancel click fails in Luma.
No direct cancel:/reject call is counted as native user input. NAV-004 remains
unadapted, and the experimental helper is not linked into the application or tests.
The native opening check KEY-001 still uses reject only for fixture cleanup.

Navigation harness corrections are separate from production fixes: QFile::copy
uses temporary names and could not prepare a 255-byte destination, so the fixture
now writes that public PNG directly. Unsupported-image tests recover by dropping
the valid original after verifying the visible error and preserved catalog; this
lets the harness's final successful-save check run without an expected error.
Repeated outside clicks may legitimately close/reopen a Dialog. The 200-click
scenario verifies balanced popup lifecycle and usable input after dismissal,
without assuming every eight-click burst leaves the popup open.


### Thumbnail unit-fixture completion and error observation

`navigation-presets-final-ctest.log` retains a failure of
`edited-thumbnail-snapshot-cache`: the old unit fixture timed out while polling
Image.Error after directly completing a controlled response from the GUI thread.
The initial cause is not proven. Fifteen diagnostic repeats passed; Qt 6.10.3
already has a finished-before-connect guard, so no lost-signal production defect
is asserted. An initial added Connections observer also failed to count the
transition; its handler now names the observed thumbnail explicitly.

The unit fixture now queues its controlled completion to the response's owning
image-reader thread, after request registration, and records the exact Error
transition and source URL. It first asserts the production retry interval is
1500 ms, then lengthens that interval only in the fixture because this test
already invokes the retry signal manually to check the source binding. This
separates that binding test from automatic retry timing. The production thumbnail
provider, PhotoCard timer and retry behavior are unchanged. Original failed runs
remain retained; repeat verification does not retroactively turn them into PASS.

The final serialized-completion fixture passed 30 consecutive targeted repeats
(`thumbnail-serialized-run.log`), followed by full CTest with 38 PASS and one
optional private Nikon SKIP (`navigation-presets-clean-ctest.log`, 91.02 s).
This is verification of the normalized fixture; it does not prove the cause of
the initial timeout.

## Additional stress, race and recovery fixture corrections

The original `expanded-first-report`, `expanded-stress-first-report`,
`stress-input-report`, `expanded-functional-final-report` and
`expanded-oracles-report` remain retained, including their failures.

- A click does not give macOS Slider keyboard focus. The editing stress fixture
  now reaches it with real Tab input under process-local Qt TabFocusAllControls;
  no OS preference is changed. QQuickSlider does not implement the assumed
  Home/End behavior, so the 1,000-gesture case uses actual endpoint pointer input.
  The 500-edit cases retain full documents after every edit and verify all
  200 retained Undo and 200 Redo states, including exact final preview pixels.
- PRV-008 now waits for each photoOpened-triggered asynchronous Library reload
  and compares the final asset against the UUID saved before activation. The
  original composite final assertion did not identify which check failed; its
  catalog already contained the expected Photo-0099. A three-repeat rerun passed
  with separate load, activation, UUID and pixel assertions.
- The import-cycle test's own read-only sqlite observer originally had no busy
  timeout. Three retained failures explicitly reported SQLITE_BUSY (code 5) at
  sqlite3_prepare. Only this observer now uses a 1,000 ms busy timeout; production
  locking and the five-second GUI watchdog are unchanged. IMP-019 then passed
  three native repeats, each with 50 cancel/reimport cycles and durable session,
  item, asset and cleanup oracles (`import-cycle-fixed-report`).
- REC-008 now proves the post-source-deletion Contrast render has a new URL and
  distinct expected pixels before Undo, then requires a new URL restoring the
  original pixels. Already-visible old pixels cannot satisfy either transition.
  REC-009 corrupts only a hash-guarded private cache after the prepare process
  exits and before the verification process constructs its image provider.
- Held Qt callback tests preserve actual FIFO delivery and verify their gate was
  caught. File-dialog selection remains an explicit fixture boundary; it is not
  counted as native chooser selection. Ordinary angleDelta wheel input is not
  trackpad or momentum coverage. Small public images are functional fixtures,
  not substitutes for 20 MP performance workloads.

## Scene readback and local performance

`current-perf-first-report` retained focus failures. Performance setup now raises
and activates the native window before timing, requiring active/exposed state
again after the measurement. No global window or OS preference is changed.
`current-perf-focus-report` still failed ADV-012 despite exact current document,
Ready URL and provider pixels. Retained same-size 926 × 1532 RGBA8 frames differ
in 92 pixels / 95 channels, with maximum absolute channel difference exactly 1.
This establishes the observed discrepancy; attributing it to Qt/Metal
quantization is an inference, not a reproduced application rendering defect.

Only scene readback comparisons allow an absolute difference of one RGBA8 level;
document and provider pixels remain exact. Before/after frames must differ beyond
that tolerance, preventing an old frame from passing. Geometry and non-null item
guards precede grabWindow. NAV-002, PRV-002, ADV-012 and PERF-006 passed on an idle
host in `checkpoint-perf-guard-final-report`. Measurements include declared
input/debounce/polish boundaries; they do not measure physical scanout. PERF-006
uses a native foreground-only local run; FLT-002 retains the separate CI functional
path. The Qt Quick Metal UI is not the planned 0.6 GPU image-processing backend.

## StandardKey.Quit differs between Cocoa and offscreen

Hosted PR run 38922231206 at commit 8a1efbb was cancelled at its 50-minute limit.
Build/unit tests passed, while several exit/restart cases waited for a shortcut
that was never delivered. Local `GITHUB_ACTIONS=true` reproductions retain the
same issue in `offscreen-quit-repro-report` and `offscreen-quit-binding-report`.
The latter records an active/exposed window, enabled ApplicationShortcut and
actual QKeySequence::keyBindings(Quit) containing Exit. Qt 6.10.3's offscreen
theme uses WindowsKeyboardScheme; hardcoded Ctrl+Q does not match that binding.

Driver::quitShortcut now sends the actual resolved StandardKey through QTest and
the unchanged production QML shortcut. It never calls root.close(). Cocoa
separately asserts Cmd+Q is declared and sends that exact combination. The raw
key-input helper is unchanged. Actions record the platform, resolved binding and
whether native Cmd+Q was exercised; no fake ApplicationState is injected.

The local offscreen/software simulation passed 45 results, including 150 actual
empty-app launches (`offscreen-quit-fixed-report`). A separate Cocoa/Metal suite
passed 54 results including the native Cmd+Q/restart paths and three production
fixes (`cocoa-quit-final-report`). Both passed provenance, source consistency,
original SHA and SQLite integrity. GITHUB_ACTIONS=true here is a local environment
simulation, not proof of a hosted CI pass. The CI deadline remains unchanged;
the next pushed commit still needs its own hosted result.

## Window / WB / export draft normalization

`window-wb-export-first-report` retains 9 native PASS and 5 FAIL results with
unchanged source/binary, original SHA and valid SQLite snapshots. Four failures
were fixture assumptions; the numeric negative-zero defect is LUMA-UX-020.

- APP-014 requested 899 px through QWindow::resize and received an actual 899 px
  window despite minimumWidth=1000. That programmatic request bypasses the native
  user-resize constraint; it does not prove a real macOS resize handle can do so.
  The invalid clamp assertion and its original screenshot remain retained. This
  ID is unregistered again until real native resize input or another valid
  acceptance boundary is available; supported-width-only checks are not a full
  pass for the specified 899/900/999/1000/1199/1200/1450 contract.
- Public nextItemInFocusChain does not predict every actual focus-scope/proxy
  route. WB now uses bounded real Tab input until the target has active focus,
  checking document invariance on every step, rather than requiring predicted
  intermediate object identities. No direct focus setter or OS preference change
  is used.
- The WB prepare process closed its window successfully, but the old fixture
  required Library.working=false. That aggregate intentionally includes the
  closing state. Shutdown now checks no live import, no busy/save and a closed
  window, matching the established restart contracts.
- EXP-009 attempted to decode an empty source string before an asynchronous
  Library metadata page was ready. Reference preparation now waits for the
  actual nonempty row source and loading=false. The same guard protects the
  immutable queue oracle. No production decoding/export behavior was changed.

The first native run already passed 50 fullscreen round trips at 1000 × 640 and
50 at 1200 × 760, preserving exact edits, selection and three nonzero scroll
positions. It also passed protected original/catalog/WAL/SHM aliases, live edits
after immutable export planning, actual SQLITE_FULL autosave/Retry and real
Library wheel input overlapping a running 1,000-item export queue. These are
focused results, not full-scope release qualification.

After the formatting fix, `window-wb-export-fixed-smoke-report` retained an
additional numeric fixture failure at the third inserted preset. The label had
already correctly shown positive zero for -0.001 and 0. An intermediate fixture
added wheel input and required the actual clicked signal, but all three attempts
missed that click (`window-wb-export-final-report`, 30 PASS / 3 FAIL). These raw
failures are retained; no production preset behavior was changed in response.

The fixture now waits for ListView.moving=false and a layout polish before
testing whether the row centre is inside the clipped viewport. It then requires
one actual clicked signal. `numeric-scroll-settled-report` passed three full
80-label runs: the third row was at y=84 inside a 101 px viewport, contentY=27,
moving=false. All -0.001/0/+0.001 document and durable values stayed exact.
This verifies the normalized input boundary; the earlier missed hit is not
attributed to one unproven internal Qt cause.

The other ten WB/recovery/export cases passed three times in
`window-wb-export-final-report`. The final corrected-source fullscreen cases
each passed 50 native round trips (`fullscreen-fixed-final-report`, two Metal
results, run `20261017T062228Z-7607`). These non-blocking REGRESSION cases also
passed their first 50-cycle runs before the label correction. Offscreen may not
qualify native fullscreen: the adapter explicitly requires Cocoa. A subsequent
serialized full build and CTest passed, with 38 PASS and one optional private
Nikon SKIP in 83.74 s (`window-wb-export-full-ctest.log`).

## Gesture fixture semantics and actual keyboard focus

`gesture-remaining-first-report` retains four PASS / four FAIL native results.
The focused-control clipping was reproduced separately three times and fixed as
LUMA-UX-021. Two other failures were fixture assumptions:

- After a named WB choice, the existing currentIndex binding can display Custom
  (index 6). Up from there selects Fluorescent, not As Shot. The corrected reverse
  input is actual Home; the pinned Qt ComboBox implementation activates item 0
  for that key. No direct currentIndex setter or WB production change was used.
- Preset JSON serializes only selected groups, so decoding an Optics-only preset
  restores neutral defaults for unselected groups. Its independent expected
  document now explicitly copies only typed lens state and its enabled bit into
  a default document. In-memory Copy correctly retains the full captured document;
  both operations are still required to change only Optics in their targets.

DTL-007 remains unadapted. The initial test held an actual Option/Radius Slider
gesture and moved to a preset, but hover preview did not arrive while the Slider
owned the exclusive pointer grab. This is an input-feasibility boundary, not
proof of diagnostic persistence in production. The failed input trace is retained;
no direct HoverEvent, preview setter or simulated response is counted as coverage.

The corrected seven new cases and twelve affected existing cases passed three
native repeats in `gesture-import-final-report` (57 Metal results). Full build
and CTest then passed: 38 PASS, one optional private Nikon SKIP, 84.59 seconds.

## Hosted import-Cancel race and physical Copy checkpoints

Hosted run 39008653483 at ca38254 completed build/unit tests, then reported
281 blocking PASS / one FAIL. IMP-004/repeat-3 observed progress=0.953 at its
90% checkpoint, but the import completed before Driver::click's next frame.
The Cancel button was therefore unavailable. Retained SQLite evidence has two
cancelled sessions and one completed session; catalog integrity passed and the
main-loop maximum stall was 96 ms. No production import failure is asserted.

The original 57,572,076-byte evidence ZIP was downloaded through the GitHub
connector and verified against its supplied SHA-256:
`ecaf4c32843a6b7e858272a2d4610910f9ec4988aa466e644bab62fe841bf37f`.
Original reports, actions, screenshots and failed-case SQLite remain locally
retained under `ci-ca38254-evidence`.

The process-local test VFS now holds the real writer after the exact owned Copy
item 99/499/899 has been published. The actual Cancel or Quit input runs while
that writer is held; release then permits normal atomic completion/cancellation.
The final asset delta must be exactly the known boundary or one item below it,
with real copied files and no leaked temporary output. Default position 0 keeps
the established single-copy fault/recovery scenarios intact. This is functional
checkpoint coverage; pausing a worker does not measure production cancel latency.

An initial local observer incorrectly also required delivered GUI progress to
equal the physical checkpoint. LibraryModel throttles updates to 100 ms in the
worker; a held worker need not emit that next update. Its first timeout and a
partly run second attempt remain retained in the interrupted
`import-cancel-offscreen-fixed` run. The author stopped that known-invalid suite;
the runner cleaned up its own child. It is not a completed qualification.

Only that invalid GUI-progress condition was removed. The physical checkpoint,
real UI action, strict retained rows, integrity and cleanup remain mandatory;
observed GUI progress is logged separately. A local GITHUB_ACTIONS=true
offscreen/software simulation passed IMP-004/006 and APP-008 three times (nine
results, `import-cancel-physical-offscreen-report`). The subsequent native
57-result suite also passed those cases and the default-position import fault,
50-cycle and process-death recovery regressions. A new hosted commit run is
still required; the local environment flag is not a hosted CI result.


## Long-filename visual oracle and sidebar focus transition

The first ten visual/recovery scenarios passed in `visual-remaining-first-report`
(run `20261017T124640Z-19791`). Manual screenshot review nevertheless found the
left sidebar overflowing beneath the filmstrip for a 255-character filename.
The initial oracle checked the toolbar/card/source label, but not preset action
reachability. Its recorded PASS remains intact; a strengthened three-failure
reproducer establishes the production defect LUMA-UX-022.

The first layout correction passed pointer activation but its new keyboard check
failed (`sidebar-overflow-fixed-smoke-report`). Closing Copy restores its focus;
manual scrolling deliberately does not reveal an unchanged focus item. The fixture
now leaves through actual Tab and traverses back, without direct focus setters.
That fresh transition also failed three times in `sidebar-focus-transition-report`:
the first implementation deferred the same helper twice with different pane
arguments. The final implementation defers one callback which checks both panes.
The acceptance waits for the actual clipped bounds before Space activation.
No source-label truncation or manual contentY assignment was added to the fixture.

VIS-005 imports 100 malformed owned images. Production reports contain a source
line, an error and blank separators, so this is a declared larger-than-100-line
readability workload, not a claim that 100 failed items render as exactly 100 lines.
The actual complete text, line 100 and both endpoints are checked through real
wheel input. VIS-004 independently exercises exactly 100 export-failure lines.
Neither case is an alias for the global rendered-text overlap audit VIS-008.


## Auxiliary-window polish acknowledgment in hosted CI

Head `3e4fc19` hosted run `39033565439` completed with 281 blocking PASS and
one DSC-001 timeout; build and all available unit/integration tests passed.
The original artifact ZIP is retained with SHA-256
`8ce08c1aa7f0bdd992886ddb2a05612dcea5119cc5b68d042c54909e8dde4774`.
The failed process had a live heartbeat, no hard hang, four main-window frame
intervals, valid SQLite/original integrity and no QML warnings.

DSC-001 creates a separate real ScrollView positive-control window. Its Driver
requested an update of that window but waited for Metrics.polishes, attached
only to the main window. Unrelated main-window updates could acknowledge it;
otherwise the five-second wait expired. Driver.frame now observes the actual
window's afterAnimating through a scoped QObject connection established before
update. The timeout, hidden-window handling and production code are unchanged.
This remains a GUI animation/polish acknowledgment, not physical scanout proof.
The corrected scenario passed four final-source Cocoa attempts and ten local
CI-environment offscreen repeats (`visual-driver-final-report`,
`sidebar-driver-native-final-report`, `driver-frame-offscreen-final-report`).
Provenance, source consistency, original SHA and SQLite passed. This local
simulation is not a hosted run; the original hosted CI FAIL remains retained.

The intermediate left ScrollView also needed an explicit scrollbar gutter:
`sidebar-gutter-repro-report` retains three native failures showing actual
Flickable/source-label bounds overlapping the full vertical scrollbar hit area.
Only the new left pane reserves effectiveScrollBarWidth; a fading thumb cannot
hide the geometry assertion. Right-pane behavior is not changed by this finding.


## Import/scroll fixture required a fully visible card unnecessarily

The first Library block (`library-remaining-first-report`, run
`20261017T132538Z-32470`) passed five cases but stopped LIB-017 before Cancel:
its anchor helper required an entire card inside the viewport. Import progress
reduces the viewport height, and a legal settled offset can straddle both edges
without one whole card fitting. The screenshot retains substantially visible
cards and a working grid, not a production blank state.

The helper now requires a real asset at least half visible with its center inside
that viewport, logging the actual bounds and identity. The independent post-reload
identity/distance assertion is preserved. Three corrected runs then reproduced
the actual reload jump, tracked separately as LUMA-UX-023. The stronger final
fixture additionally verifies insertion on both sides of the anchor, using its
new independent Catalog rank rather than its previous numeric row.


## Held scrollbar coverage for the unpublished viewport correction

Independent review found a missing edge in the first LUMA-UX-023 correction:
a reload could schedule restoration after a drag/scrollbar press had already
started. The earlier movement-start handler would not fire again for that input.
A supplemental native probe held the first eligible Library queued delivery
after final progress and eight durable insertions, pressed the actual scrollbar thumb,
and observed reloadStarted with both native Qt mouse buttons and ScrollBar.pressed.
`library-held-scrollbar-repro-report` retains its failed restore-target assertion.
This tests the actual import/UI path, without a direct model refresh or fake input
state. A cacheBuffer page may deliver at that queued boundary; the separate
pressed-at-reload observation proves the actual import/input overlap either way.
Capture now ignores a current drag/held scrollbar; a post-release real
scroll is the positive control. The initial two 2,048-source Z/A insertion waves
remain separate proofs that ordinary completed input preserves asset identity.


## Hosted functional success without a build-attestation file

Hosted `56e4e28` run `39036376277` completed successfully: 282 blocking attempts
passed, as did build, unit/integration, format and clang-tidy. The retained ZIP
SHA-256 is `eeea223928875e1698cc635d12a666b0397aaa264e8ddfa2feb3542c2a20b5c3`.
Source consistency and original hashes passed; all 279 native-case catalog-integrity
results passed (the three suite-wide integrity results have no catalog).

Its report nevertheless marks build provenance UNVERIFIED: ordinary cmake build
never created `build/ux-build-provenance.json`. These functional PASS results
remain intact, but are not a formally attested release qualification. Local
current-scope evidence uses build_ux.py and retains matching source/binary proof.
The workflows now call the existing build wrapper immediately before UX and
require matching provenance through --require-build-provenance. Release
qualification implies the same strict preflight. Missing or stale proof is
rejected before fixtures/child processes; setup and suite drift are also checked.
The evidence upload includes the build record. All 25 runner unit tests passed
in 5.468 seconds, including 22 mocked CLI matrix cases using the real JSON
validator. A real stale-build preflight also returned exit 2 before execution
(ci-provenance-preflight.log). These tests validate runner accounting, not native
UX or hosted CI. The prior hosted functional results are unchanged; a new hosted
attested run is still required.

## Batch/update SDK callback and close-overlap qualification

Eight new cases passed three times on Cocoa/Metal in batch-update-final-report
(20261017T180206Z-54876); ten affected existing cases passed three times in
batch-update-affected-report (20261017T180407Z-56461). Source/binary provenance,
source stability, original hashes and all catalog snapshots passed. Full build,
CTest (38 PASS, one optional Nikon SKIP, 92.13 seconds), 25 runner tests,
format/diff and four focused clang-tidy checks passed. No new production defect
was reproduced in this block.

UPD-005/006 opt into a test-target NativeUpdater factory before Context
construction. The real UpdateController receives the same error/found callback
sequence as its Sparkle adapter and the UI receives real Check/held-Slider input.
The fixture does not execute Sparkle, network, appcast, archive installation or
restart delivery. Its measurements explicitly identify this boundary. Six further
offscreen/software PASS (batch-update-offscreen-report, 20261017T180522Z-57220)
are a local CI-environment simulation, not a hosted result.

BAT-004 holds an actual WAL write while 25 real Cancel inputs arrive. A late
COMMIT may win atomically; only unchanged full documents or the exact complete
transaction are accepted, with corresponding revisions/history and a subsequent
normal Apply/Undo positive control. BAT-014 checks all permanent table counts and
checkpointed main pages through 100 Apply/Undo pairs; it does not claim an RSS
plateau or observation of the worker's TEMP journal. APP-011/RACE-010 overlap
actual autosave with an export waiting for that save. RACE-010 holds and resumes
the genuine queued exporter closeReady while the editor emits its own source;
it does not synthesize closeReady or claim cancellation during encoder writes.

## Export settings/privacy qualification and a locked native session

The first three native cases passed in export-settings-first-report. After a
later user continuation, all nine final-source attempts failed before scenario
actions at the common native-window activation guard. Computer-use inventory
independently reported that the Mac was locked and automatic unlock had failed.
These nine failures remain in export-settings-final-report (20261017T222606Z-64260);
they were not relabeled as product defects or overwritten. The user then
explicitly confirmed manual unlock.

With the same attested final source, nine native Cocoa/Metal attempts passed in
export-settings-unlocked-final-report (20261017T222930Z-65253), followed by
18 affected native PASS in export-settings-affected-report (20261017T223110Z-65856).
Separate local offscreen/software cohorts passed 9 + 18 attempts. All source/
binary proof, unchanged source, source SHA and catalog integrity checks passed.
The native wrapper used a process-scoped caffeinate display/system-idle assertion;
it did not change lock preferences or bypass manual unlocking. Full application
build, 25 runner-accounting tests, format and four focused tidy checks passed.

EXP-017 executes both overwrite and deletion of its own original watermark after
planning, with an independently defined red pixel rectangle in both real outputs.
EXP-025 uses two processes and literal complete settings plus a valid stable UUID;
both last-used and named-profile exports retain full durable documents and source
identity. EXP-027 reads its own known JPEG and all four actual PNG policies through
primary ImageIO APIs, checking exact GPS coordinates, IPTC locations, copyright,
capture and lens tags in properties/metadata and forbidden values in XMP.

## Hosted attestation path verified

Hosted commit 3183ff9 run 39056014400/job 117464018541 passed all steps: build,
39 available unit/integration tests (115.54 seconds), attested UX wrapper,
282 blocking attempts, format and tidy. Artifact 12110895076 ZIP SHA-256 is
91de1a17b6473b5da2515d16c091fbb5d69ca1ace0c918d6dab25d69bf94987a.
Its source hash 15363caa5a7bc1b5397f0a9fa38bf4923de2e365b9bcea3d57f4db9faf0886fb
matches the local clean 3183ff9 snapshot; source_commit identifies the expected PR
merge checkout 90e23e5793e75c148b225e77d3f6d0be68b0cf77. Build provenance, source
consistency, original SHA and all 279 native-case catalog-integrity results passed.
The 92 future cases remain NOT_IMPLEMENTED, and the blocking subset remains
NOT_QUALIFIED for the full current-scope release. Earlier missing-proof artifacts
are retained unchanged.

## Export queue stress and unqualified AppKit lifecycle probes

EXP-023/024 passed three times on Cocoa/Metal in export-stress-final-report
(20261017T225237Z-70937), followed by 18 affected native PASS in
export-stress-affected-report (20261017T225511Z-71714). Both complete workloads
also passed once in a local offscreen/software simulation. Build provenance,
source stability, original SHA and all catalog snapshots passed. Full build,
25 runner-accounting tests (5.62 seconds), format and focused tidy passed.

All 500 inputs are private byte-distinct copies. The seeded schedule, complete
saved documents and original identities are persisted before execution; every
actual output is decoded and compared with its saved-document CPU reference.
EXP-024 performs all 100 Start/Cancel cycles at the real post-install/COMMIT
boundary, retaining each committed first output while its queued completion is
held. It verifies all unstarted outputs and temporary files remain absent.
The fsynced fixture journal is test evidence; production export queues remain
session-local. Neither mid-encoder cancellation nor independent mathematical
validation of the renderer is claimed.

The first combined report (cocoa-export-stress-first-report,
20261017T224040Z-68475) retains two stress PASS and three AppKit lifecycle FAIL.
APP-016/017 and SLD-009 never reached genuine miniaturized/application-inactive
notifications despite key-window loss. A real app.exec experiment retained the
same failures in cocoa-event-loop-first-report (20261017T224533Z-69368), so that
main-loop change was reverted. The Slider had already released automatically
and retained its full document; no production Slider defect was established.
These three prototypes remain unregistered and outside verified coverage.
Qt already requests Regular activation policy for bare executables; absence
of an app bundle alone is not an established cause. All raw failures remain.


## Panel/Detail stress and hosted 25bfec8

PERF-017/018 and DTL-011 passed three times on Cocoa/Metal in
panel-detail-final-report (20261018T072955Z-79020); seven affected cases passed
three times in panel-detail-affected-report (20261018T073124Z-79528). Three
complete local offscreen/software workloads also passed. Provenance, source
stability, original SHA and SQLite integrity passed throughout. Full build,
25 runner tests (5.67 seconds), format and two focused tidy checks passed.
The first format command named a nonexistent Python script and exited 2;
the actual existing check-format.sh then passed. No native attempts were
reclassified or rerun for that orchestration typo.

Each panel case preserves exact preexisting Undo/Redo branches, complete saved
settings/store bytes, queue, baseline output pixels and all private originals
through 200 actual cycles. Import exercises the QML Dialog Cancel button, not
NSOpenPanel. DTL-011 executes 200 independent keyboard edits in nine controls,
then 200 Undo and 200 Redo. The last natural catalog-associated xWrite is held
while real Export enters waitingForSave; the fixture does not identify that fd
as specifically WAL. No memory plateau, latency threshold or independent CPU
renderer mathematics claim is made. No new production defect was reproduced.

Hosted 25bfec8 run 39075666008/job 117522182165 completed all steps successfully:
38 unit/integration PASS, one optional private Nikon SKIP (141.46 seconds),
282 blocking UX PASS, build/provenance, format and tidy. Artifact 12117387978
ZIP SHA-256 is d09b48c7292cffb87c82511a74953492ddbb259fd7131003b1506baf5852efa4.
Source/binary proof, unchanged source, original SHA and 279 native-case catalog-integrity
results passed; three global integrity results have no catalog. The 92 future
results are NOT_IMPLEMENTED and also have no catalog. Expected PR merge checkout
is 4545586ddfe32d73c5689519d1cbc0d1c77eb940; source hash is
d7a321ac6dbb29c72a94fe53977d2e305badce678e65b2c9f73b55bf28df0cee.
The blocking subset remains NOT_QUALIFIED for the full current release gate.


## Thumbnail retry/invalidation and native Retina mapping

LIB-006/018 and MAC-001 passed three times on Cocoa/Metal in
thumbnail-retina-final-report (20261018T074529Z-82693); seven affected cases
passed three times in thumbnail-retina-affected-report (20261018T074623Z-82997).
Both Library cases also passed once in local offscreen/software
(thumbnail-offscreen-report, 20261018T074756Z-83546); Retina never uses that
fallback. Provenance, unchanged source, original SHA and SQLite checks passed.
Full build, 25 runner tests (5.65 seconds), format and four focused tidy checks
passed. The unavoidable Cocoa WId/NSView ARC bridge has a narrowly documented
interop suppression; no production defect or behavior change was introduced.

LIB-006 observes actual 1500 ms Timer signals and Image.Error/source/retryCount
for 0, 1 and 2, then a 3250 ms quiet interval with no third retry. Real selection
and bounded ScrollBar thumb movement preserve the same error delegate. Restored
private bytes and a genuine saved edit create a new generation with retryCount0
and correct pixels. LIB-018 holds a real production cache reader across actual
scrolling and a saved edit; all three repeats observed exactly one targeted
invalidation and zero model resets during the edit. Every visible Image node,
including edge cards, is read back with QPointer lifetime/source guards. Exact
cache pixels supplement declared scene-resampling mean RGB tolerance4/255.
Full documents/revisions and source SHA are protected for all 64/200 assets.
This does not observe the identity/order of OLD Qt response delivery after
NEW Ready; LIB-004/005 and RACE-002 remain unqualified.

The initial combined report (thumbnail-retina-first-report,
20261018T073906Z-81285) retains one LIB-018 PASS and two fixture failures.
Diagnostic report thumbnail-retina-diagnostic-report (20261018T074107Z-81853)
confirmed that a single synthetic QTest angleDelta120 did not move this grid,
and that the Retina helper had assumed backing coordinates inherited NSView's
flipped basis. Logical(.25,.75) actually converted to backing(.5,1678.5) while
round-tripping exactly. The helper now derives backing origin/directions from
actual AppKit conversions of logical corners. The retry case uses a genuine
bounded scrollbar drag for its specified responsiveness contract; OS wheel
delivery is not qualified by that case. Original reports remain untouched.

MAC-001 requires active Cocoa/Metal, actual NSWindow/QWindow/QScreen scale2,
odd1101x801 logical and2202x1602 physical scene dimensions, independent native
coordinate roundtrips, mapped image/letterbox pixels and rendered gold handle
centroids. The input boundary is Qt QTest window dispatch; physical scanout,
hardware pointer events and sublogical hardware precision are not claimed.


## Scoped history chaos and immutable replay

UND-017 completed 10,000 actual semantic actions on Cocoa/Metal with seed 123456
(history-chaos-first-native-report, 20261018T075617Z-85273), then a full immutable
replay in another process (history-chaos-replay-report, 20261018T080432Z-87248),
and a fresh seed 0 (history-chaos-seed-zero-report, 20261018T081133Z-88845). The
first two workloads took 417.322/417.956 seconds; the replay plan and entire full
checkpoint measurements were byte/value identical. Seed0 generated another plan
and final document. All three native results passed full document/history,
original hashes, natural durable saves, exact Ready preview and SQLite checks.

A separate local offscreen/software simulation executed all 10,000 actions
(history-chaos-offscreen-report, 20261018T081832Z-90459). Six affected restart/
recovery/update/thumbnail cases passed three times (history-chaos-main-affected-
report, 20261018T082544Z-92058). All reports have matching source/binary proof,
source stability, original SHA and catalog integrity. Source hash is
32676c4ff9e40a8163510e752856960a4ccee966c621f86e4791a7321704faf4.
Full build and CTest passed 40 available tests with one optional private Nikon
SKIP (87.51 seconds); format/diff and four focused tidy checks passed. The new
QtCore-only oracle tests validate cap/no-op/branch, full typed hash field groups
and deterministic plans/replay. Five Python routing tests and the 26th runner
argument bridge validate infrastructure, not native UX.

The default plan SHA is 31d7f503e573e6c30e10a663d85083ac78f22c29d77789cdab627c6259ecac5d,
with final document 84318b30df0e6093fd49e5f3a767bd81a1036d5b6cbe3e219b9d341c16d9bb3d.
Seed 0 plan SHA is fe70acf217477500b924c77757e49e30367f9d40d9d78b8cf9f23232f54baaa8;
its final document is 804c44a549b798ce588ed950f13579827ef5113e5e8ae0a359bb006b7f50f486.
The full plan/seed exists before Context; intent is fsynced before each input and
observed full state afterward. Every 100 actions and at completion, natural
autosave, exact current-source Ready pixels and SQLite integrity are required.
No endEdit/saved helper repairs acceptance. Numeric inputs use real focused
Sliders; history uses real keyboard shortcuts. The independent model is a
201-state timeline/cursor with cap 200, not production history or parameter APIs.

A real native negative probe changed only app version in a structurally valid
private replay copy, updating its checksum. Native header regeneration rejected
it with expected exit1 before Context/window/catalog/seed creation. Original
replay bytes remained unchanged. Raw child result is intentionally FAIL; the
separate assertions.json records expected rejection, never native UX PASS.
This probe remains under history-chaos-native-preflight-rejection.

The scope is three Basic integer controls, discrete edits and Undo/Redo only.
It is not the general Chaos Monkey, pointer/modifier gesture pool, injected
worker reordering, 100k stabilization gate or future phase 0.6. Counts below 10,000
are explicit smoke/subsets and cannot replace the release baseline. No new
production defect was reproduced in this block. The process-scoped caffeinate
assertion covered the validation supervisor, including software checks before
returning to native input, and expires with that process.

Hosted 4fc89dc run 39107703186/job 117617029069 completed all steps successfully:
38 unit/integration PASS, one optional Nikon SKIP (168.93 seconds), 282 blocking
UX PASS, build/provenance, format and tidy. Artifact 12129573574 ZIP SHA-256 is
de608a1215bf3d2e466d127a80fe0b9e68950fc1922256c27a656988a92d66cf.
Source/binary proof, original SHA and 279 native-case catalog-integrity results passed.
Source hash 009be3a715f95c76144c6fc79ade5f57f17b0db40d3d191fe9e30d467130c105
matches local clean 4fc89dc; expected PR merge is 15a4bb3e696345b9be96acf199dae12e8e4e5d72.
The blocking subset and 92 future NOT_IMPLEMENTED results do not qualify the
full current-scope release. Earlier evidence is retained unchanged.


## Held Detail diagnostic and preset hover: unqualified native boundary

The DTL-007 candidate was independently reviewed for input ordering and failure
cleanup. A read-only input trace now observes actual Alt KeyPress/KeyRelease,
LeftButton release and application/window deactivation; a fresh active diagnostic
is required immediately before the real pointer move. Failure evidence is saved
before RAII input cleanup. No direct preview/diagnostic/property calls are used.

The final actual Cocoa/Metal attempt, detail-diagnostic-window-trace-report,
20261018T084247Z-96807, did not reach the required preset-hover boundary. After
moving the held pointer onto Soft Portrait, the Slider remained pressed, the
LeftButton remained held, exactly one Alt press and no release/deactivation were
observed, diagnostic remained Amount luminance, and hovered/hoverPreviewId/
presetPreview stayed false/empty/false. This demonstrates an unreachable hover
in this specific exclusive-grab fixture, not a reproduced application defect.
The adapter was removed from source registration and the suite's NATIVE set.
DTL-007 remains unqualified; verified coverage remains 358 native cases.

Two earlier raw failures are retained: detail-diagnostic-continuity-first-report
(20261018T084113Z-96228) incorrectly assumed the global Qt modifier cache stayed
Alt after QTest mouse input with NoModifier; detail-diagnostic-event-continuity-
report (20261018T084200Z-96534) initially counted the same unaccepted key propagating
through QML items. The final trace counts the window KeyPress once and checks
absence of any actual release instead. All three runs passed source/binary proof,
source consistency, original SHA and catalog integrity. None is relabeled PASS.
The unregistered final source, integration snapshots and qualification.json are
retained in the ignored pending-detail-diagnostic/integrated-unqualified archive.


## IDLE Cocoa event-loop experiment and hosted restart failure

Two unregistered IDLE probes used a queued single public AppKit action and a
real local QEventLoop::exec; main's control flow was unchanged. Both native
experiments failed their required fresh transitions, cocoa-idle-candidate-report,
20261018T085016Z-99324. NSApp.running was true, Qt loop level was1 and the owned
window was actually key immediately inside each action. Nevertheless neither
performMiniaturize nor deactivate produced the required native notification or
miniaturized/inactive state during 625 read-only polls over10 seconds. Thus the
hypothesis that EventLoopExec alone fixes the previous Cocoa fixture was not
confirmed. No production defect or official APP-016/017/SLD-009 PASS is claimed.
The experimental source/wiring is archived and unregistered; bundle/companion
alternatives remain unrun. Raw FAIL and pre-cleanup state are preserved.

Hosted 29074c6 run39110825283/job117626404846 completed build, unit/integration
and attestation, then failed its blocking subset:281 PASS, one SLD-020 FAIL and
92 future NOT_IMPLEMENTED. SLD-020's first verify child completed its visible
quit path but terminated nonzero; repeats2/3 passed. The raw message is empty,
and no exception or crash stack is present in its app.log. The old supervisor
did not retain the child exit code and let an empty child message suppress its
fallback. A signal or production root cause cannot be reconstructed from that
record. Original failure evidence remains unchanged. Artifact12130972625 ZIP
SHA-256 d8c69297207ca0efda8320edd91e72ac7618038673e4232df5be46b429913b1e
was independently verified. Report provenance, source consistency, original SHA
and the failed case's SQLite snapshot check passed; this is not a green CI run.

The supervisor now retains child_returncode, child_reported_status and an
untouched child-result.json before enriching or overriding result.json. Abnormal
exit after an apparent PASS remains FAIL, with numeric exit code/signal fallback
when the child supplied an empty message. Real isolated subprocess tests exercise
positive exit, SIGTERM after writing PASS, preserved nonempty assertions,
watchdog-killed apparent PASS and verified intentional SIGKILL checkpoints.
These are infrastructure checks and never native UX qualification.


Targeted follow-up: restart-exit-diagnostics-offscreen-report,
20261018T181334Z-3071, completed30 full SLD-020 quit/restart attempts in a
**local offscreen/software simulation**. All60 prepare/verify processes exited0,
reported PASS and retained their raw child-result.json; full source/binary proof,
source consistency, original SHA and catalog integrity passed. The hosted
abnormal exit was not reproduced by this cohort and remains unresolved.
The diagnostic runner's29 infrastructure tests passed (5.893 seconds).
CI and release artifact lists now also retain the untouched child-result.json.

The attempted native follow-up is separately retained as
restart-exit-diagnostics-native-report: three activation timeouts before the
scenario could run. The macOS tool explicitly confirmed a locked Mac; a new
manual unlock request is pending. These failures are environment evidence,
not native restart PASS and not an application defect. No further native input
is attempted until the user confirms the Mac is unlocked.


## Verified kill checkpoint accounting

A further supervisor review found that a valid kill-ready marker could exempt
an abnormal process exit even if the child had already exited before the parent's
final poll and SIGKILL delivery. A marker proves the intended persistence
boundary, not process termination. The runner now accepts the kill variant only
when both the validated marker and actual returncode -SIGKILL are present.
Normal exit0, positive exit7 and SIGTERM in the marker/final-poll race all remain
FAIL. intentional_kill_requested is retained separately from intentional_kill.
A deterministic mocked process covers that race without signaling any real
process; the real isolated-child checkpoint test still checks actual SIGKILL.
All30 runner infrastructure tests passed (5.923 seconds).

## Owned-volume environment preflight, not UX qualification

A private64 MiB HFS+ blank disk image with NONE partition layout completed two
real attach/detach generations, preserving the same VolumeUUID and nonce file
on remount to the same unique /Volumes path. Attach plist, own-device diskutil
info and actual statfs device/filesystem identities were checked before every
detach. Final attachment is absent; the image and raw commands/plists remain
under owned-volume-env-cxfr_puq. No user volume was inspected or operated on,
and no physical USB-removal behavior is claimed. MAC-007/008 remain unadapted.

The first preflight, owned-volume-env-hpperxya, failed before creating or
attaching an image: this host's hdiutil blank-create interface rejects -format.
The corrected command uses the documented -type UDIF blank read/write type;
filesystem, size, layout and hdiutil interface are unchanged. Both raw records
are retained. This is an environment fixture correction, not an app defect.

Verified-kill runtime follow-up: verified-kill-software-report, 20261018T190719Z-5607, executed UND-014/015 and REC-007 three times each in a local offscreen/software simulation. All9 validated prepare children were actually terminated by SIGKILL, all9 new verify processes exited0, and full documents, catalog integrity, original SHA, source consistency and build proof passed. This does not replace native or hosted qualification.


## Resize mode/scalar output qualification and shutdown observations

EXP-026 maps to the actual current UI: one mutually exclusive mode and one
scalar. The scalar is entered under another mode, then mode changes final →
opposite → final occur without retyping it. No simultaneous width/height fields
or derived-size Label are invented. Actual output dimensions provide the size
oracle: seven modes × landscape/portrait create14 real PNGs per attempt.
Independent literals cover fractional rounding and up/down scaling; complete
RGBA8 pixels use direct2D box integration/bilinear reference after shared CPU
development and ICC encoding, with fixed maximum channel error1/255.

The original unregistered candidate passed once in local software and once on
Cocoa/Metal. After explicit optional guards, local recipe initialization,
stride widening and reference reservation, the official adapter passed three
native attempts: resize-shutdown-final-native-report,20261018T191631Z-7773.
Eighteen affected export/restart attempts passed on Metal in
resize-shutdown-affected-native-report,20261018T191745Z-8438. The local
software cohort resize-shutdown-software-report,20261018T191911Z-8944,
passed three complete resize attempts and nine actual SIGKILL/restart attempts.
All provenance/source stability/original SHA/SQLite checks passed; source hash
18ced70de6dd2de58bcb32f5a8bf5e9a23e113987dce315cc173c638504cc9f8.
Four infrastructure CTests passed in8.38s (including30 runner tests), and format
passed. Focused tidy found only an empty diagnostic catch in SqliteFault; adding
an explicit snapshot-unavailable stderr marker fixed it, and its focused tidy
passed. That catch is outside the normal observed path; case semantics and
acceptance remain unchanged. Full evidence is retained before this log-only fix.

Natural RAII stderr markers bracket Context and QGuiApplication owner destruction;
no reset, scope/control-flow change, sleep, drain or ownership fix was added.
All39 normally exiting processes recorded every boundary, exit0 and zero wrapped
SQLite files in the destructor snapshot. Nine verified SIGKILL prepares rightly
do not execute destructors. These observations do not prove future jobs absent
and do not establish the cause of hosted29074's abnormal exit. The raw original
failure remains unresolved; latest hosted5af765f CI is a separate pending run.

The macOS tool subsequently confirmed the desktop was unlocked, so native tests
resumed. No unlock bypass or security/preference change was made. EXP-028 remains
unqualified: deleting the original image watermark after planning correctly uses
the immutable staged copy, as independently tested by EXP-017. That design/spec
conflict was not relabeled an execution-failure PASS.


## Owned removable-source recovery and first-page readiness

The isolated MAC-007 fixture imports host control A and mounted source B through
actual Add options/Start. A new private 64 MiB HFS+ disk image is created without
sudo, force detach or arbitrary-device discovery. Its exact attach plist, backing
inode/device, UUID, mount statfs identity and nonce sentinel are checked before
each storage action. Ordinary detach removes the real pathname; the same image,
mount path and UUID are remounted. Explicit checked final detach is required
before PASS. Failure cleanup never guesses a device or turns an uncertain outcome
into success. No physical USB/hardware-input equivalence is claimed.

The scenario verifies actual failed source reread, preserved control session,
complete saved documents/revisions/metadata, import journal, UUID/path/checksum,
exact Ready main-preview pixels, actual thumbnail Image-node readbacks and exact
saved-document JPEG cache pixels. It exposed LUMA-UX-024 before any card helper
could retry lazy metadata reads. Original three blank-card native FAIL records
and the original failed unit remain retained.

The subsequent thumbnail wait was a fixture error: invalidating B increments a
global URL counter but only notifies B. Actual unchanged A legitimately retains
its previously notified generation0, while a fresh model getter returns1.
Diagnostic `20261019T064438Z-14131` records both real Images Ready, their bindings,
model URLs and retry0. The corrected wait follows each actual notified binding
and bounded retry; exact saved-document cache pixels and scene readback remain
required. That original fixture failure is not relabeled as a production defect.

Two further Cocoa failures remain in volume-source-full-boundary-first-report
(`20261019T070308Z-14971`) and volume-source-full-boundary-confirm-report
(`20261019T070407Z-15140`): afterAnimating timed out during/before Add input,
although the window reported visible. Main-loop watchdog, original SHA and SQLite
checks passed and each own volume was normally detached. Contemporaneous native
exposure/occlusion evidence was absent, so the cause remains unknown. No render,
activation, event-loop or timeout workaround was introduced. New before/after
fixture Qt snapshots record exposed/active/appState separately. The next complete
native PASS, volume-source-native-window-diagnostic-report, has after-attach
active=false/appState2 but exposed=true; inactivity alone cannot explain the
older failures. Three further complete native candidates passed in
volume-source-qualified-candidate-native-report (`20261019T070952Z-16982`).
A complete local software run also passed (`20261019T070514Z-15334`); it is not
a hosted or native rendering result. Provenance/source/original SHA and SQLite
checks passed throughout; candidate coverage is registered only after these runs.

The full application build and 40 available CTests passed, with one optional
private Nikon skip (101.67 seconds). Focused tidy exposed new helper descriptor
control-flow diagnostics; explicit throwing branches now prevent negative file
descriptors from reaching fsync/fstat. New helper warnings were fixed without
unrelated production refactoring. Existing header/old-code advisory diagnostics
remain distinct from changed-line diagnostics.

## Hosted diagnostic checkpoint and local storage cleanup

Hosted 5af765f run39156735777/job117761405504 completed every step successfully,
including build, unit/integration, attestation, 282 blocking attempts, format and
tidy. Artifact12147529232 is 57,079,454 bytes; ZIP SHA-256 is
744792e826b9584ef0c7c2c15c0f9167a3dcc2a2a4a471f65fc73355ad161577.
Source hash1452d437301f476bcaa646bb86d9fbaba821a2ccd6495bf5263ce953cbf5541d
matches local clean5af765f; PR merge checkout906865eb3c4cc222804b7cb57ab2352c85501deb
is expected. All 465 raw process stages have their expected terminal status;
SLD-020 prepare/verify exited0 in all three repeats. The 279 native-case
catalog-integrity results are report checks, not 279 retained SQLite snapshots:
all-PASS jobs do not preserve those databases. The earlier unexplained hosted
29074c6 SLD-020 failure remains unresolved and is not relabeled PASS.

At the user's request on2026-10-19, 47,799 obsolete generated test copies,
old CMake build files and reproducible Swift/Python caches were removed. The
measured disk-space increase was7,774,834,688 bytes. All source hashes including
uncommitted work stayed identical. Latest successful cohorts, all non-PASS
payloads, raw JSON/actions/logs/metrics/screenshots/SQLite evidence, public master
fixtures and stable0.4.14 packages were retained. The exact deletion plan and
per-file pre-deletion SHA manifest are in ignored .tools/cleanup. Historical
reports still describe their original execution; removed generated PASS inputs
are regenerable and must not be claimed to remain present on disk.

The user's subsequent direction defers load testing: only explicit functional
quality cases now run. Stress/soak/chaos/performance and large-catalog repeated
workloads are reserved for the pre-release campaign of 1.0.0. Their earlier
evidence is historical; deferred
checks remain unexecuted, not passing, in future qualification decisions.


Final registered-source functional checkpoint: volume-source-final-native-report,
20261019T071449Z-17632, passed MAC-007 plus SEL-002, FLT-001, IMP-001, SLD-020,
PRS-001 and EXP-001 three times (21 Cocoa/Metal PASS). No stress, soak, chaos,
performance, large-catalog or repeated-load scenario was selected. The local
software counterpart passed MAC-007/SEL-002/FLT-001 once each,
volume-source-final-software-report,20261019T071619Z-18703. Source hash for both is
e0a45f7db84de541dec98c43907a76f72dcc74832a5d5923cfb7824d22780102.
All27 actual child stages exited0 with untouched child PASS. Four final own-image
cases checked15 successful exact-owner storage commands, two accepted attachment
generations, two ordinary detaches and absent final mountpath. Provenance, source
consistency, original SHA and catalog integrity passed. Manual scene review of
the actual missing-source state confirms retained A pixels/metadata, selected B,
visible missing B pathname and both cached thumbnails. The infrastructure subset
passed (30 runner cases plus metric accounting,6.03 seconds); format and focused
new-helper/scenario tidy passed. The full CTest run above preceded the user's
load deferral; no full load-bearing campaign was repeated after that direction.
