# Final 1.0 readiness — NOT QUALIFIED

The human postponed all builds, tests, static analysis and QA to final preparation.
This document records the handoff requirements; it is not a test report, waiver
or permission to publish. The stable release remains 0.5.1. Do not tag 1.0.0 or
update its Sparkle channel until the complete campaign, fixes and retests finish.

### Independent hosted checks after a failed gate — 2026-10-23

[Hosted evidence](qualification/2026-10-23/ci-memory-pressure-recovery.json)
captures `e5850c1`: build/model/unit stages PASS; the blocking UX step fails,
and eight subsequent independent UX stages PASS. This confirms the CI condition
correction, including executed PhotoCard and thumbnail regressions. The job is
still running at this snapshot; exact blocking-case counts and full static
results are not yet available. No full CI PASS or cause for the blocking result
is inferred. Future runs upload the small blocking summary immediately before
long supplemental checks; the complete archive remains unchanged.

### Catalog restore and destination protection — 2026-10-23

[Evidence](qualification/2026-10-23/catalog-restore.json) adds BKP002/006/007 to
the official adapter set. Together with manual backup, all four cases pass
**12/12 offscreen and 4/4 Cocoa/Metal DPR2**. A real edit after backup makes the live catalog different;
restore reproduces the selected snapshot's edits, revision and photo identity.
Corrupt input publishes no destination, and restoring over the active catalog
is explicitly refused. Existing backups, originals and the live document/history
remain unchanged. No temporary leftovers or QML warnings occur.

This exercises the production restore handler through a declared FileDialog
selection fixture, not native source/destination pickers. Fresh native confirmation passes all four cases without QML warnings or original/
catalog integrity failures; the one-minute interval is complete and Mac is free. Full build,
5 affected CTest checks, 11 policy tests, formatting and affected blocking tidy
pass. CI/release run all four cases three times and retain reports. The current
inventory has 52 applicable blocking scenarios without adapters; neither that
inventory count nor these bounded checks qualify the outstanding release scope.

### Native metadata and XMP lists — 2026-10-23

[Evidence](qualification/2026-10-23/metadata-xmp-lists.json) records a confirmed
ImageIO interoperability defect: creators/keywords can be returned as arrays of
`CGImageMetadataTag`, while Luma required bare strings. Both external packets and
Luma's own written lists failed to read. Unwrap only string-valued tags and retain
strict structure/type checks and the 256-item limit. Native unit regression
covers Unicode, ordered/repeated creators, unique keywords, exact roundtrip,
normalized scalar spelling and rejection of nested/oversized lists.

Six new metadata/XMP UX adapters pass **18/18 offscreen and 6/6 Cocoa/Metal DPR2**:
Unicode form save/reopen, catalog-only storage, explicit XMP write/read,
confirmation cancellation, external-write conflict and malformed input. Original
hashes, catalog integrity, edits, history, revision and selection remain intact;
no QML warnings occur. The native interval is complete and Mac is free. Timing
is MEASURE_ONLY; a 2398 ms native stall is retained, not counted as a latency pass.

Full application build and **62 CTest checks PASS**, with one SKIP for the absent
private Nikon RAW. Formatting and affected blocking clang-tidy pass; nonblocking
warnings remain. Initial harness Tab-input and XML-fixture failures are retained
separately from the reproduced production defect. These checks do not qualify
crash/disk-full recovery, native file pickers, load or the complete phase 0.9.
Full hosted CI and all remaining release gates are still required.

CI and release workflows explicitly run all six metadata/XMP cases three times
and retain their report. Only two belong to the blocking inventory; the four
regression cases must not disappear from hosted coverage. This addition keeps
existing failure propagation and source attestation requirements.

### Cancel Import and catalog maintenance — 2026-10-23

[Evidence](qualification/2026-10-23/import-cancel-maintenance.json) records a
confirmed production defect: Cancel Import left its cancellation token set, and
later backup/restore, XMP and relink operations reused that token. Actual
LibraryModel reproduction passes backup before cancellation and then fails with
`Operation cancelled.`. An otherwise identical no-cancel control passes.

Maintenance now has a separate token, cancelled by Request Close/destruction.
The integration regression verifies backup/restore, explicit/pending/automatic
XMP, metadata readback, file/folder relink and preserved identity, edits,
revision, metadata, collection membership and original hash. A deterministic
close-before-worker control confirms that shutdown still cancels maintenance
without publishing a snapshot or accepting new work.

New blocking UX-P09-BKP-001 passes **3/3 offscreen and 1/1 Cocoa/Metal DPR2**.
Actual Settings and Back up now clicks create readable snapshots with current
saved edits and retain previous good copies. Live document, selection, history
and originals stay unchanged; no QML warnings or integrity failures occur.
This bounded case does not qualify restore dialogs, retention rotation,
disk-full/crash recovery or load. The native interval is complete; Mac is free.

Complete application build, **7/7 affected CTest checks**, **40 runner tests**,
**11 policy tests**, formatting and affected blocking clang-tidy pass. The
duplicate local full-tree tidy scan was deliberately stopped before source
edits and remains INCOMPLETE; its log is retained. Full hosted CI/static and
all other outstanding release requirements remain open. No 1.0 publication.

### Hosted CI and memory recovery — 2026-10-23

[Verified evidence](qualification/2026-10-23/ci-memory-pressure-recovery.json)
records hosted `21b39ac` CI: full build, model numerical/source/photographic
checks and unit/integration pass (**61 PASS / 1 private Nikon SKIP**). All
**204 executed blocking UX repetitions pass**, including ZOM001 3/3 after the
async CPU-oracle correction. The stage correctly exits INCOMPLETE: 58 applicable
blocking adapters were absent and 29 load scenarios were deferred. This is an
incomplete qualification, not an executed-case failure or an overall CI PASS.

The workflow now runs independent UX and static checks after any earlier
failure, provided their build prerequisite succeeded and the run was not
cancelled. No UX `continue-on-error` or acceptance waiver is added. Every failed
step still fails the job. The job ceiling is 180 minutes to allow the model,
regression and full static sequence to finish; per-case limits are unchanged.

The next two adapters expose a real recovery defect: normal pressure restores
the controller's zoom availability and cache budgets but did not notify QML.
Both warning and critical controls reproduce an enabled controller with disabled
buttons and zero state notifications. The fix notifies on recovery without a
new render. Identical post-fix checks pass **6/6 offscreen**; three reclaimable
cache budgets/entries reach zero under pressure and restore correctly. Pixels,
document, catalog revision, history, selection and original hashes remain intact.

These are bounded injected-handler checks. Their owned UX controller stops and
drains OS notifications before prescribing the transitions. Initial attempts
with live OS notifications are retained: genuine host warning disabled zoom
before the intended test. This does not qualify physical memory stress, RSS,
OS delivery, or AI/retouch cancellation.

The fresh approved quiet interval passes **6/6 native** on Cocoa/Metal Qt scene,
Retina DPR2: PRV012, ZOM003/004/012 and MEM001/002, with exact CPU photo references,
no QML warnings and intact original/catalog checks. Mac was released. Pinch uses
Qt native-gesture delivery, not a physical trackpad. Full application build,
5 affected CTest checks, 40 runner and 11 policy tests, whole-tree formatting
and affected blocking clang-tidy pass. A previous full tidy scan was invalidated
by concurrent source edits and stopped; its log is retained, not counted PASS.
Full hosted static analysis and the remaining release scope still need evidence.

### Viewer scale and input — 2026-10-23

[Evidence](qualification/2026-10-23/viewer-scale-and-input.json) records the
implementation of Fill, manual 1–1600% (including 2:1), pointer-anchored wheel and
pinch zoom, smooth/momentum pan, and double-click Fit/100%. Scale changes on an
already full-resolution image do not start another render or change saved edits,
history or selection. New-photo, pressure and full-resolution failure reset Fit.

The first input runs exposed incorrect scrolled-content coordinates for wheel
and pinch. Scene-to-viewport conversion fixes both; queued wheel events preserve
their pending bounded transform. Full-to-full anchors are exact; the rounded Fit
raster and 1:1 backing-pixel snap have explicit half-source-pixel budgets.

Two subsequent 1600% runs hit the unchanged five-second watchdog. Disabling
image smoothing did not fix this and was reverted. Inactive tool Canvases were
following the 49152×32768 magnified image. Keeping their geometry zero/tiny while
zoom is active makes the same scenario pass, with original smoothing retained.
Their dimensions are now asserted. Original failures and incomplete stack
samples remain in the evidence; no watchdog or integrity limit was relaxed.

The affected regression passes **51/51 offscreen/software DPR1** repetitions,
with no QML warnings or original/catalog integrity failures. Full application
build, **9/9 affected CTest checks**, **40 runner tests**, **11 quality-policy
tests**, keyboard import/edit/export and eight-language preset dialogs pass.
All eight languages also keep the new controls within a 1000×640 window.
Affected formatting and blocking clang-tidy checks pass; nonblocking warnings
remain. PRV012 enters qualification through an explicit implemented-target phase
mapping; its original TARGET specification is unchanged. CI/release require all
four new input adapters, three repetitions each, with retained evidence.

Fresh native confirmation of all four input adapters now passes in the follow-up
above. Synthetic Qt native-gesture delivery is not physical trackpad qualification. PRV011 and
ZOM006–009 overlay mapping at arbitrary zoom remain open; tools still use Fit.
GAP049 is therefore not closed. Hosted CI on `21b39ac` finishes with 204 executed
UX repetitions PASS but an INCOMPLETE coverage gate, as detailed above. No overall
CI or 1.0 release PASS is inferred.

### Native transient confirmation and zoom-oracle worker — 2026-10-23

The subsequent [PhotoCard readiness control](qualification/2026-10-23/photocard-final-readiness.json)
demonstrates that identical small interactive/final pixels do not prove final
callback completion. The test now waits for the existing pending-histogram flag
to clear before pinning its preview URL. A real held final result proves the old
predicate insufficient and the new predicate rejects it; release delivers the
same current callback exactly once and changes the URL without changing pixels.
The full bounded caption workflow passes **3/3 offscreen** with unchanged
document/history/catalog/source assertions. No product code changed. This does
not attribute or close the original aggregate BUG045 failure; native and hosted
confirmation of this follow-up remain pending.

[Evidence](qualification/2026-10-23/native-preview-zoom-worker.json) records
**5/5 native PASS** on clean `bdea5bd`: SLD016/017, PRS017 and PRG007/008,
Cocoa/Metal, Retina DPR2, with no QML warnings or catalog/original-integrity
failures. The approved three-minute quiet interval is complete; Mac was released.

[Hosted CI on bdea5bd](https://github.com/yakut-sekonts/Luma/actions/runs/39932371084)
passes model preparation, numerical/source/photographic checks, full build and
unit/integration. Blocking UX returns **203 PASS / 1 FAIL**, plus 87 SKIPPED and
31 NOT_IMPLEMENTED. ZOM001 repeat 1 exceeds the unchanged five-second main-loop
watchdog; repeats 2/3 pass. Supplemental and static stages are skipped.

The failed sample contains a single LCMS frame on the GUI thread. Both passing
sibling samples identify the synchronous test-only `reference -> renderCPU ->
DisplayTransform` path; their maximum loop stalls are 2678/3573 ms. This supports
test-oracle blocking as the cause, but the failing stack itself is incomplete.
The original failure and verified artifact digest remain retained.

The independent zoom CPU oracle now runs in an async worker using immutable
fixtures/settings/profile data. Real GUI events and heartbeats continue while
waiting; timeout cancels and joins before fixture lifetime ends. Exact pixels,
ICC/geometry and watchdog limits are unchanged. All eight affected scenarios
pass **24/24 offscreen repetitions** without warnings or integrity failures.
The adversarial runner control still kills a hung process despite its forged PASS.
Formatting and affected blocking tidy checks pass; nonblocking warnings remain.

Native confirmation above precedes this worker-only harness change. The replacement
hosted run now passes ZOM001 3/3; this is not an overall release PASS. GAP049,
BUG045 and all other unqualified scopes remain open. No 1.0 publication.

### Transient preview ordering and preset pixel oracle — 2026-10-23

[Retained evidence](qualification/2026-10-23/transient-preview-preset-oracle.json)
records the complete bounded offscreen baseline: **264 PASS / 1 FAIL**, plus
427 SKIPPED and 78 NOT_IMPLEMENTED inventory entries. The complete CTest run
passes **61 tests**, with one SKIP for the absent private Nikon RAW fixture.
These counts do not qualify skipped workflows or hardware/native behavior.

The PRS017 failure reproduces in three runs with failure-only pixel capture.
Each actual UI image equals fresh actual Metal replay byte-for-byte; the expected
image equals the independent CPU replay. Exactly one RGB byte differs by one,
with exact alpha, and the linear error stays below the existing fixed budget.
Preset readiness now shares the established WB/numeric Metal rounding allowance:
RGB <= 1 only for enabled, GPU-compatible processing; CPU-only/incompatible paths
remain exact. Alpha, geometry, channel format, color space and DPR remain exact.
Negative controls reject violations, including equal bytes with different metadata.
No product pixel math or complete document/history/persistence assertion changed.

New PRG007/008 adapters verify actual delayed final delivery during Original/
Processed transitions and preset hover. Current hover completion is accepted
without saving; completion after leaving hover is rejected once by the original
revision guard. Both Original directions, final CPU pixels, histogram, saved
revision, selection, history and original hashes are checked. The UX-only probe
reuses actual results and original callbacks without blocking the render worker.
The renderer is explicitly CPU to isolate ordering; native Metal describes the
Qt scene. CI and release require both cases with three repetitions.

The final affected regression passes **42/42** bounded preset/WB/transient
repetitions, with workload-only PRS012/019 explicitly skipped. New native
confirmation is pending a fresh quiet interval; no permission is inferred from
previous completed intervals. Source/build identities are in the linked evidence.
This regression is separate from the preceding full baseline.
The independent hosted run remains a required gate; no overall CI PASS is inferred.

The source audit also confirms **LUMA-GAP-049**: requirements §16 specifies Fill,
2:1, arbitrary scale, wheel/pinch zoom and double-click 1:1/Fit, while the current
Viewer exposes only Fit/100% plus pan. Missing behavior is an implementation gap,
not merely an absent adapter. BUG045 and the remaining qualification gates stay
open. Source remains 0.9.0, stable distribution 0.5.1; no 1.0 artifact is published.

## Qualification started — 2026-10-21

The source-completion handoff is now being qualified on `codex/1.0-qualification`
(base `c375740`). Local environment: Apple Silicon arm64, 8 GiB RAM, macOS
27.0.1 (26A434), Qt 6.10.3, RelWithDebInfo, Python 3.12.15. After the human
installed Xcode 27.1 (27A9275), accepted its license and completed first launch,
both model compilations and the full application/PLCrashReporter build succeeded.
Xcode tools use `DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer`;
the existing C++ build still uses its recorded Command Line Tools compiler and
macOS 27.0 SDK. This is a qualification build, not the final signed artifact.

The first pass exposed a QML syntax error, a backup-helper/member name collision,
strict Objective-C++ compile errors, missing editor includes and test link
dependencies. These were fixed with the original compiler warnings enabled.
New regression coverage exercises creative settings/snapshots/rendering/Auto
Adjust, smart rule queries/revision conflicts/backup restore, and advanced import
Copy/Move with secondary backups and frozen metadata/develop settings. Existing
historical-catalog fixtures and future-preset rejection cases must represent the
current schema 21 and preset codecs 8/9 without weakening their invariants.

Intermediate logs are retained locally under `.tools/final-*`; only completed,
retested checks count as passing. This section does not close the full campaign.

### Library scroll and cancellation follow-up — 2026-10-22

[Evidence](qualification/2026-10-22/library-scroll-and-cancel.json) retains the
original failures, source/binary hashes, local report locations and retests.
These runs use the local offscreen/software backend, not the native macOS UI.

- BUG-035 now has a mandatory supplemental CI/release regression with 176 small
  PNGs. Selection, open/return, filename filtering and metadata paging preserve
  the actual visible asset and offset in three repetitions. A newly reproduced
  10 px return shift is fixed by retaining the mouse-selected viewport after
  focus transfer. Arrow-key reveal remains active. Two targeted QML mutations
  fail the expected offset/model-reset assertions.
- Adjacent Tab/Return/keypad activation passed 6/6. DSC006 passed 3/3 after its
  obsolete requirement for a Library reload was replaced with observation of
  the actual prior/busy/photoLoaded transitions. Viewport geometry, controls,
  rendered edits and durable settings remain checked in both directions.
- [CI on 406f161](https://github.com/yakut-sekonts/Luma/actions/runs/39627660731)
  passed model conversion/numerical/source/photographic checks, app build and
  unit/integration tests. Blocking UX returned 194 PASS / 1 FAIL, plus 29 SKIPPED
  and 92 NOT_IMPLEMENTED entries. These are repeat/result counts, not unique
  completed specification scenarios. The failure was TRN003 repeat 2: Cancel
  unavailable; subsequent supplemental and lint stages were skipped.
- The old TRN003 adapter passed 10/10 locally, so the CI failure was not locally
  reproduced. Its generic QObject MetaCall interception could hold an unrelated
  preview/save delivery. TRN003/TRN004 now use the existing test-only actual
  successful Auto-result gate and a calibrated 320×240 fixture. Real Cancel
  input precedes delivery; the same operation must be rejected exactly once by
  the original stale-revision guard. Both cases pass 3/3; no production Auto
  implementation or cancellation guard was changed.
- Formatting, affected-file clang-tidy (with non-blocking warnings), 40 runner
  tests and 10 quality-policy tests passed. Post-fix hosted CI is pending.

The subsequent human-authorized native interval passed **18/18** repetitions on
Cocoa/Metal with Retina DPR 2: Library scroll 3/3, Tab/Return/keypad activation
6/6, DSC006 3/3 and TRN003/TRN004 6/6. No QML warnings were recorded. All reports
retain the clean `ff2bcb0` source/build attestation and original-file integrity.
See [native evidence](qualification/2026-10-22/native-scroll-cancel.json).
The chooser remains a fixture boundary. Physical trackpad/wheel input, other
window sizes, additional filter/sort/load coverage and the remaining final
campaign stay open. Stable distribution remains 0.5.1.

### Recording, filmstrip and hosted keyboard follow-up — 2026-10-22

[Review of the supplied recording](qualification/2026-10-22/scroll-recording.json)
identifies the original surface as the horizontal Develop filmstrip, with 0.5.1
visible in the window. The earlier grid-only regression did not qualify this
surface. A new 176-image case reproduces a 24.82 px partial-card shift in current
source; compact PhotoCard now preserves `contentX` after mouse focus. A subsequent
check found Right-key index advancement without revealing the next card; explicit
Left/Right containment fixes it. The test checks the actual anchor/offset,
single activation/load, no reset, frame-synchronized positions and navigation in
both directions across the metadata-page boundary.

Final local offscreen retests: grid **3/3**, filmstrip **3/3**, keyboard **6/6** and
neighboring SEL017/SEL018/DSC006 **9/9**, with no QML warnings. A control restoring
the previous horizontal positioning fails the unchanged final-anchor assertion.
The intermediate property-only observer incorrectly counted a move-and-restore
inside one synchronous handler as visible motion; its failures are retained.
The final observer records both property notifications and nonempty positions at
scene synchronization. The subsequent authorized native run passes grid **3/3**,
filmstrip **3/3** and keyboard **6/6** on Cocoa/Metal, Retina DPR 2, with no QML
warnings and intact originals. Reports attest the same dirty worktree based on
`ff2bcb0`; this is distinct from the earlier clean-commit run. Physical trackpad
input and complete native UX qualification remain open.
See [exact evidence](qualification/2026-10-22/filmstrip-keyboard-followup.json).

[Hosted CI on ff2bcb0](https://github.com/yakut-sekonts/Luma/actions/runs/39762926272)
passes model conversion/numerical/source/photographic checks, application build,
unit/integration tests and blocking UX (**195 PASS**, 29 SKIPPED,
92 NOT_IMPLEMENTED result entries). Supplemental keyboard returns **5 PASS /
1 FAIL**; later supplemental and lint stages are skipped. Its last keypad run
retained a destroyed PhotoCard pointer even though both actual Tab directions
reached the expected UUID. The observer now resolves the current card by UUID
without setting focus or mutating the model. A declared QML control that recreates
the delegate passes with one observed instance replacement. The original failure
and artifact digest are [retained](qualification/2026-10-22/ci-ff2bcb0.json).
The post-fix hosted run remains pending; no full release gate is closed.

### Sky photographic interiors and runtime contracts — 2026-10-22

[Raw report](qualification/2026-10-22/sky-photographs.json) and
[bounded visual observations/provenance](qualification/2026-10-22/sky-followup.json)
record **3/3 source and 3/3 native photographic cases PASS**. Conservative regions
were annotated before prediction on the existing licensed day/night/indoor PNGs.
The indoor portrait has no mask pixels at or above 0.5; day/night sky interiors
pass the fixed confidence/coverage criteria and annotated river, rocks and tower
interiors pass the background limits. Unannotated pixels remain unknown.

The application's real CoreMLSky provider is compared with independent PyTorch
resize plus the pinned source network. Maximum source/native difference is
0.001969, below 0.0005 plus the unavoidable half-step of the provider's uint8 mask
(0.5/255). This does not loosen the separate Float32 Core ML numerical gate.
Native progress/input preservation, unsupported selections, malformed input,
pre-cancel, cancellation after resize and exact replay after cancellation pass.
Seven oracle controls reject constant/inverted masks, concealed false positives,
weak confidence and malformed annotations/probability planes.

CI and release packaging now require this check and preserve its report, native
logs and comparison sheets. Initial local build, affected clang-tidy, formatting
and workflow YAML parsing pass; independent hosted execution is pending.
No production Sky code, weights or conversion recipe changed. The three sheets
were inspected, with soft/coarse fine edges and missed gaps in foliage recorded.
Dense boundary accuracy, RAW and broader scene coverage, crop/orientation mapping,
CPU fallback, actual accelerator placement and resource limits remain open. The
full 1.0 gate remains **NOT QUALIFIED**.

### Bounded functional fixes and GitHub distribution — 2026-10-23

[Retained evidence](qualification/2026-10-23/functional-release-fixes.json)
records the original local functional baseline (**240 PASS / 16 FAIL / 188
SKIPPED / 326 NOT_IMPLEMENTED**), both intermediate failing retests and **48/48
passing repetitions** of those sixteen scenarios after fixes. These are local
offscreen/software results, despite the backend selector recording
`hosted_ci: true`; no new native-window interval is included.

Add without a backup now accepts legal existing long filenames. Generated
Copy/Move/backup names retain their validation, and file-open errors identify the
actual failed path. Directory notifications refresh only cached Missing roles
asynchronously: they no longer reset the Library, destroy delegates or move its
viewport. A real file removal/restoration test checks both role changes, zero
resets, selection, documents and source integrity. Import, Library and new Metal
unit tests pass 3/3; the shipping QML smoke test and probe-symbol isolation pass.

The Metal HSL calculation now avoids cancellation near black/white. Seven actual
Metal cases meet the unchanged linear error budget and final one-step RGBA8
budget, with exact alpha. Combined curves/HSL on HDR still exceeds that budget
and is explicitly routed to the unchanged CPU implementation; two fallback
cases preserve exact CPU pixels. This is a correctness/performance tradeoff,
not a claim that the combined Metal path is numerically fixed. Original failed
numeric experiments and unchanged exact UX pixel assertions are retained.

UX fixtures now use the current retouch-aware cache identity, destination layout
and twelve preset groups. Cancellation holds the actual successful load result;
export waits on an actual pending WAL write; crop input acquires the real corner
grip. Per-field/card, model-reset and failed pixel evidence remains enabled.
Affected static analysis completed with nonblocking warnings; formatting and
workflow parsing pass. The 100k Library contract passed separately, but was run
alongside UX and supplies only functional bounds, not performance acceptance.

[Hosted CI on 1bcec07](https://github.com/yakut-sekonts/Luma/actions/runs/39774239377)
passes model and application/unit stages, blocking UX, keyboard/scroll/dialog
and gradient-handle regressions. PhotoCard localization fails 3/3 on the obsolete
cache identity; subsequent stages are skipped. After that correction a local
run reports 2 PASS / 1 FAIL at a separate aggregate state invariant. Failure-only
diagnostics were added; subsequent runs pass 10/10 and 20/20 without reproducing
it. This intermittent observation remains open; those passes do not explain it.

The owner defines the final release as GitHub open-source publication. Packaging
and publisher tests accept truthful ad-hoc, non-notarized 1.0 artifacts while
requiring Sparkle signatures, bundled models and verified asset identities.
Developer ID/notarization are optional. Full QA, installation and update
qualification remain mandatory; stable distribution is still 0.5.1.

### Initial qualification baseline — 2026-10-21

[Machine-readable baseline](qualification/2026-10-21/baseline.json) records the
source commit, per-case outcomes and hashes of retained local logs.

- 54 background CTest cases passed (`final-offscreen-tests-1.xml`), including
  the full app's ordinary-image, import, edit/export/restart and QML workflows.
- Three native CTest cases passed in a human-approved uninterrupted interval:
  eight-language UI, complete masks and local gradients, including restart/export
  checks (`final-native-tests-1.xml`, 150 seconds). This is not the complete UX spec.
- The private Nikon RAW case was SKIPPED without its configured fixture; it is
  not counted among the 57 passing cases.
- New coverage checks creative controls, smart collections, advanced import and
  deterministic retouch donor selection. Historical fixtures now construct
  coherent old catalogs before migrating to schema 21; version-rejection tests
  distinguish valid mask codec 3 / preset codecs 8–9 from future codecs.
- Fixed QML errors in catalog maintenance, SmartRuleEditor conditions and wrapped
  confirmation dialogs. Thumbnail tests wait for the requested asset's loaded
  metadata page; an empty asynchronous placeholder no longer counts as invalidation.
- Both `.mlmodelc` resources were compiled, inventoried and installed in Luma.app
  and the UX bundle. Their manifest remains `qualification: NOT_RUN`.
- Native CPU numerical comparison against the adapted source networks is **FAIL**:
  Sky passed all three synthetic inputs (max absolute error 2.39e-7); Denoise
  failed all three (max error 0.2002, RMSE up to 0.00486). Tolerances remain
  max error 0.0005 / RMSE 0.00005. HDR/negative-pixel passthrough passed.
  See [retained numerical evidence](qualification/2026-10-21/model-numerics.json)
  and LUMA-BUG-036. No model-quality gate is closed.
- The first Python prediction probe crashed in `libcoremlpython` / `_PyObject_Free`
  on the Core ML reset queue. Its stack matches the upstream NumPy-owner lifetime
  bug described in [coremltools #2829](https://github.com/apple/coremltools/pull/2829).
  The retained numerical probe uses a separate native process with Core ML-owned
  tensors; it does not patch the pinned converter or hide a crashed process as PASS.
- Formatting passed. The full clang-tidy pass covered 251 unique translation
  units and exited 1 with 13 blocking diagnostics in 10 files. All affected files
  were fixed and passed targeted retests, along with the added native model probe
  and changed regression tests. The original failing log is retained; this is a
  full initial pass plus affected-file retests, not a second full clean run.
  Non-blocking analyzer warnings remain.
- After the merge worker ownership change, the application rebuilt and its
  affected QML shell, batch/history and preset regressions passed (3/3).

Formatting-only changes were separated in `e18a2d6`; functional fixes and evidence
follow separately. The baseline above does not cover all new 0.6–0.9 workflows,
the 23 legacy gaps, photographic AI quality, load/stress or signed distribution.

### Denoise follow-up — 2026-10-21

Adaptation v2 fixes the independently reproduced native downsampling error using
checkpoint-equivalent spatial slices/1×1 convolution. It also preserves exactly
uniform RGB tiles to prevent the source model from inventing structure on them.
Five adaptation tests passed (algebra, channel/spatial order, uniform preservation,
nonuniform processing and HDR/negative-pixel preservation); the four toolchain
preflight tests and native-probe clang-tidy also passed. The application rebuilt
with v2 installed and verified in both bundles. Six affected packaging, update
and offscreen smoke CTest regressions passed; they overlap the earlier baseline.
See [follow-up evidence](qualification/2026-10-21/model-followup.json).

The expanded comparison remains **FAIL**: [CPU 9/11](qualification/2026-10-21/model-numerics-v2-cpu.json)
and [ALL 10/11](qualification/2026-10-21/model-numerics-v2-all.json). Dark nonuniform
inputs exceed unchanged limits. Source-only checks also reproduce bright
artifacts on dark inputs, recorded separately as LUMA-BUG-037. LUMA-BUG-036 is
not closed. No photographic, installation, update or whole-campaign gate is
waived by the numerical improvement. Historical v1 reports remain unchanged.

### Source-quality gate and CI compatibility follow-up — 2026-10-21

The independent macOS 15.7.9 / Xcode 16.4 [v2 CI report](qualification/2026-10-21/model-numerics-ci-v2.json)
also passes 9/11 numerical cases and fails dark noise/impulse. Source quality now
has an independent mandatory CI/release gate against analytical clean images:
[5/7 inputs pass, overall FAIL](qualification/2026-10-21/denoise-quality-v2.json).
Both dark failures extend into the runtime's interior crop. Seven oracle tests
pass, including rejection of identity, bright, over-smoothed, invalid and isolated
amplified outputs. This gate does not replace photographic qualification and
does not modify the current model or relax numerical limits.

CI additionally exposed unsupported `std::jthread` / `std::stop_token` usage in
the diagnostic writer on Xcode 16.4. A mutex-protected stop flag and joined
`std::thread` preserve queue draining without that library dependency. The app
and new real-writer lifecycle test build locally; the test passes. Affected
clang-tidy passes its blocking checks, with existing nonblocking warnings.
Xcode 16.4 validation must still come from the next CI run. No new native UI
session, signed installer or release was exercised in this follow-up.

### DRUNet v3 and current UX fixtures — 2026-10-21

The source-quality failures persisted in an exploratory Baseline SIDD model, so
it was not adopted. Bundle v3 instead uses the author's hash-pinned MIT DRUNet
color checkpoint, equivalent down/up-sampling and a per-tile Haar noise estimate.
Original weight bytes are unchanged. The same analytical-clean gate now passes
[7/7 source cases](qualification/2026-10-21/denoise-quality-v3.json), including
the two dark failures. The native outputs also pass that quality oracle and
source comparison on [CPU 11/11](qualification/2026-10-21/model-numerics-v3-cpu.json)
and [ALL 11/11](qualification/2026-10-21/model-numerics-v3-all.json). Maximum
errors are 1.44e-6 and 3.46e-6 respectively, below the unchanged 0.0005 limit.
This resolves the exact BUG-036/037 reproductions for the replacement model;
photographic/noise/texture/tile acceptance remains open. Eleven adaptation tests
and seven independent quality-oracle tests pass. Both application bundles contain
the same verified v3 manifest; eight affected CTest regressions pass.

[CI on 5ef81d3](https://github.com/yakut-sekonts/Luma/actions/runs/39616268298)
built successfully on Xcode 16.4: 58 CTest cases passed and the private Nikon
fixture was skipped. This closes the reported BUG-038 build failure. Its UX
step still failed (15 PASS, 180 FAIL, 29 SKIPPED, 92 NOT_IMPLEMENTED), often because
chooser fixtures wrote retired LibraryPane properties. Tests now assign the
actual ImportDialog, use all 12 preset groups through visible UI clicks, and
scroll to the recapture control before clicking. The real DiagnosticsController
is included in the UX context with logging/capture disabled like unattended
production startup; the rerun has no QML warnings.

Four targeted import/selection/export cases passed in offscreen/software. A
dependent eight-case run passed seven, then the recapture scrolling correction
passed its single affected retest: twelve distinct cases have passing results,
not a full suite pass. Two UX bundle/isolation CTest checks also pass. Formatting
and affected clang-tidy pass blocking checks; existing nonblocking warnings remain.
The local offscreen runs explicitly used the existing `GITHUB_ACTIONS=1` backend
route; their `hosted_ci` metadata therefore does not mean they ran on GitHub.
An earlier broad CTest invocation was stopped after it included native UI tests;
its partial results are not used as additional qualified native coverage.
See [follow-up record](qualification/2026-10-21/drunet-ux-followup.json).

The new commits still need independent CI. Full new-workflow/native UX,
photographic model quality, resource/load/recovery and signed distribution gates
remain open. Stable remains 0.5.1; there is no new installer, tag or appcast.

### Photographic Denoise and runtime tiling — 2026-10-21

The first photographic campaign exposed LUMA-BUG-041: bundle v3 passes only
four of six noisy photo cases despite nine passing source/native comparisons.
With weak added noise, its mean-based Haar estimate over-smooths the portrait
and landscape; channel errors and landscape seam/gradient errors increase.
[The failure report](qualification/2026-10-21/denoise-photographs-v3-failure.json)
is preserved. Bundle v4 replaces that mean with a lower absolute median,
calibrated to Gaussian sigma and exported as topk/amax. The checkpoint, halo,
color conversion and acceptance limits are unchanged.

The corrected actual-runtime campaign passes [all six noisy photo cases and
nine source/native tiled comparisons](qualification/2026-10-21/denoise-photographs-v4.json).
The three unchanged photographs are observational controls, not certified
noise-free sensor references or extra quality passes. At Amount 1, each RGB
channel improves in both linear and sRGB MSE, and gradient errors improve along
both axes and actual tile seams. Maximum source/native error is 3.82e-6 against
the fixed 0.0005 limit. Native contracts also pass Amount 0 identity, Amount 0.5
linear blend, alpha/input preservation, tile progress, pre-/mid-loop cancellation
and invalid-amount rejection. Neither ALL nor these checks establish actual
accelerator placement or CPU-fallback behavior.

The synthetic gates remain [7/7 source](qualification/2026-10-21/denoise-quality-v4.json),
[11/11 CPU](qualification/2026-10-21/model-numerics-v4-cpu.json) and
[11/11 ALL](qualification/2026-10-21/model-numerics-v4-all.json). Thirteen adaptation
tests and 13 independent quality-oracle tests pass. The application build,
model installation/manifest verification in both bundles, eight affected CTest
regressions, formatting and native-probe clang-tidy pass. The photographic gate
is required in CI and release packaging, with reports and comparison sheets
retained as artifacts. [Evidence and visual-review limits](qualification/2026-10-21/denoise-photographic-followup.json)
record the exact model/probe identities, including an initial run superseded
after the probe rebuilt; only the fresh run with a stable probe is accepted.

This is bounded qualification on three small developed PNGs with deterministic
added Gaussian noise. Real camera noise, full-resolution RAW texture/color,
Sky scene quality, native UX, resource/load/recovery and signed distribution
remain open. No new native UI session or release was performed.

## Source decisions and qualification requirements

### Preview completion ordering and photo-switch zoom — 2026-10-23

P06-PRG-004/009 and P06-ZOM-005 pass **9/9** local offscreen repetitions and
**3/3** in the authorized two-minute Cocoa/Metal Retina DPR2 interval. The
existing three zoom/export cases also pass again. No QML warnings, original-file
or catalog integrity failures occurred. There is no new production behavior fix
in this batch: the existing revision guard and photo-load reset policy pass.

The UX-only probe retains the actual successful render dispatch after its final
cancellation check, freeing the sole worker for a newer render. Its original GUI
callback is later delivered and the existing revision guard rejects it exactly
once. PRG004 exercises delivery after a newer final image. PRG009 additionally
holds the next actual SessionService load and proves old-stage rejection while
the editor is still busy, before the new photo's success callback. Image/URL,
histogram, document, selection, saved revision and Undo/Redo remain correct.
Zoom switching exercises actual 100% scene pixels and pointer pan, then verifies
Fit and zero offsets on each filmstrip load while preserving saved edits.

All targets, shipping QML/probe-isolation, formatting, 40 runner and 10 policy
checks pass; blocking affected-file tidy passes with optional-access warnings.
The new probe is absent from the shipping binary. CI and release workflows now
run the preview-ordering and zoom-switch regressions and retain their evidence.
These checks explicitly use CPU image rendering; native Metal identifies the Qt
scene backend. They do not qualify GPU-command races, RAW, physical input,
external displays, latency or memory pressure. The native interval has ended.
[Evidence](qualification/2026-10-23/preview-ordering-zoom-switch-followup.json).

### Full-resolution zoom and progressive export — 2026-10-23

New P06-ZOM-001/002 adapters reproduced **2/2 failures**: Fit to 100% left
`contentX/contentY` at zero, showing the top-left corner instead of the center.
The QML callback ran before its content-size bindings settled. BUG-048 now defers
centering until those bindings settle and snaps the offset to backing pixels.
Both even and odd window dimensions are exercised.

P06-ZOM-001/002 and P06-PRG-002 pass **9/9** final local offscreen/software
repetitions. A separately hashed 3072×2048 generated chart distinguishes the
1024 interaction, 2560 Fit and full-resolution paths. The held Exposure gesture
actually displays 1024 output before release. The full provider image matches
an independent CPU document; the composited 100% scene preserves single-pixel
detail. PNG export retains all 3072×2048 pixels, declares sRGB and equals the
full-source CPU reference exactly. Saved revision/document, selection, identity,
original bytes and one-gesture Undo/Redo are preserved. No QML warnings occurred.

All targets, QML shipping smoke, test-probe isolation, 40 runner and 10 policy
checks pass. Formatting and blocking tidy checks pass; optional-access warnings
remain behind the test's throwing `require()` guard. The subsequently approved
two-minute interval passes **3/3** on actual Cocoa/Metal, Retina DPR2, using clean
`6056dd2` with matching build/source provenance. No QML warnings, catalog or
original-file integrity failures occurred. These generated PNG checks
do not qualify RAW, physical input/panning, OS pressure, latency or GPU failures.
The independent CPU oracle runs on the test GUI thread, so heartbeat stalls in
these reports are not product performance measurements.
[Evidence](qualification/2026-10-23/zoom-progressive-followup.json).

The accelerator interface also passes deterministic fault contracts: unavailable
output falls back to exact CPU RGBA/geometry, every histogram field and clipping
overlay for neutral and Noise/Sharpen/Crop/Exposure documents. Eight malformed
buffer, exception and cancellation cases reject without returning partial pixels;
subsequent renders recover and source RGB/alpha remain unchanged. The same CTest
run passes the existing 13 actual Metal numerical cases and two explicit
Curve/HSL CPU fallbacks at unchanged tolerances. These injected interface outcomes
do not qualify native command-buffer, device, driver or allocation failures.
[Evidence](qualification/2026-10-23/develop-fallback-followup.json).

### Initial phase 0.6 contracts and native confirmation — 2026-10-23

The subsequent approved three-minute interval passed **4/4** on Cocoa/Metal,
DPR 2: SLD016/017, ADV009 and the new P06-GPU-001 Settings workflow. No QML
warnings, original changes or source/provenance drift were observed. This closes
the bounded native follow-up for the WB/targeted fixes described below, without
claiming complete native QA. [Evidence](qualification/2026-10-23/phase6-native-preview-followup.json).

P06-GPU-001 also passes **3/3** offscreen. It toggles the real Metal checkbox in
both directions and checks a newly rendered preview against its CPU fixture,
complete edits, saved revision/asset identity, membership/selection, preference
persistence and one-step Undo/Redo. This is a UI/state-preservation contract;
actual GPU execution and failure injection require separate evidence.

New `preview-cache-contract` coverage passes LRU eviction, byte accounting,
immutable active ownership across replacement/purge, stale generation rejection,
zero-budget purge/resume, invalid replacements and SIZE_MAX arithmetic without
large allocations. It tests the cache directly, not actual OS memory pressure,
overall RSS or multi-hour load. All-target build, shipping smoke/probe isolation,
40 runner tests, 10 workload-policy tests, formatting and blocking tidy checks
pass; non-blocking optional-access warnings remain in the UI test.

The capability manifest now reflects source availability for 0.6–0.9. Missing
adapters become explicit SKIPPED/unadapted qualification gaps rather than future
NOT_IMPLEMENTED capabilities. No unexecuted case becomes PASS. That checkpoint
had one official phase 0.6 adapter of 58; the zoom/export follow-up above adds
three; the completion-ordering/photo-switch follow-up adds another three, for
seven at that checkpoint; PRG007/008 above bring the total to nine. Phases 0.7/0.8/0.9 still require adapters for
their 56/50/84 inventory cases, with applicability review for conditional tools.
Existing unit/model reports remain separate evidence. Release qualification is
still open, including load, hardware, installation and updater checks.

The actual `SessionService` integration additionally passes cold/warm PNG decode,
distinct standard/interactive resolutions, fresh saved edits on cache hits,
byte-limited eviction with exact regenerated pixels/alpha, full-resolution cache
reuse, zero-budget operation/resume and ownership after purge. A purge during
the real decoder's progress callback prevents late insertion into the old
generation. Cancelled reads preserve catalog state; a populated full-resolution
cache still refuses an explicitly changed test-owned source checksum. Both
cache CTest targets pass, as do formatting and blocking tidy checks. These small
isolated fixtures do not qualify OS memory pressure, RAW quality or process RSS.
[Evidence](qualification/2026-10-23/session-cache-followup.json).

The CI job budget is now 90 minutes: the prior run reached PhotoCard at about
59 minutes before failure skipped later UX/lint steps. Individual scenario
deadlines, numeric budgets and release gates are unchanged; release packaging
already has its separate 120-minute budget.

### WB, targeted gestures and native follow-up — 2026-10-23

[Evidence](qualification/2026-10-23/wb-targeted-native-followup.json) retains the
clean `41601b5` full functional run: **254 PASS / 2 FAIL**, plus 188 SKIPPED and
326 NOT_IMPLEMENTED result entries. Unit/integration checks return **56 PASS /
1 SKIP** (the absent private Nikon fixture). These counts do not close full QA.

The approved five-minute native interval passed **8/8** affected import, thumbnail,
Crop, preset and race scenarios plus **3/3** eight-language PhotoCard runs on
Cocoa/Metal, DPR 2. No QML warnings occurred and original hashes stayed intact.
BUG-045 remains open: passing repeats do not explain its original intermittent
state-invariant failure. This interval predates the fixes below.

The two functional failures are SLD016/017: actual preview differences from the
independent CPU reference are one RGB step in 48 and one channels respectively.
Actual Metal replay confirms the same differences within the existing linear
budget. The WB/numeric UI oracle now explicitly allows RGB error <= 1 with exact
alpha for Metal-enabled compatible documents; CPU-only/incompatible documents
remain exact. This changes the obsolete bit-exact UI expectation, not the
pre-existing renderer budget. Negative controls reject two-step RGB errors,
altered alpha, wrong dimensions and missing images. Document, history, durable
state and ready/source checks remain unchanged. Six extreme WB cases extend the
native oracle to 13 Metal cases and two exact CPU fallbacks, all passing.

ADV009 previously passed with four QML binding-loop warnings. Ending a targeted
gesture during photo loading was followed by MouseArea cancellation, re-entering
`endEdit`/`stateChanged`. `endTargeted` now does nothing after its gesture ended;
tests also verify that stale cancellation cannot split an unrelated edit's Undo.
The scenario now rejects binding loops. SLD015/016/017, ADV009/015 and RACE006
pass **18/18** offscreen repeats without QML warnings. ADV003 was explicitly
skipped by the quality-only workload policy, not passed. Eight affected CTest
checks, all-target build, formatting, shipping-probe isolation and affected-file
clang-tidy pass; non-blocking tidy warnings remain. New native coverage, hosted
CI, later-phase qualification, load and final distribution checks remain open.

- Denoise/Sky delivery is resolved: include both models in the installer and
  Sparkle archive. Pinned DRUNet color and U²-Net Sky sources/licenses, conversion,
  default model lookup and packaging guards are implemented in source. The
  compiled models now exist, but the installer and model qualification remain
  open. See [model delivery](BUNDLED_MODELS.md).
- Qualify the current bounded HDR/panorama algorithms. Further seam optimization,
  projection coverage or tiled processing depends on the photographic/resource
  evidence; do not advertise unimplemented 360° or generative panorama fill.
- XMP interoperability is implemented: ordered creators, exact custom label
  text, standard-color mapping, Reject as Rating -1 and catalog-only Pick.
  Fractional ratings are refused visibly, not rounded. Migration 17 and all
  import/edit/sidecar/export paths require final qualification; see
  [metadata semantics](PHASE9_DEVELOPMENT.md#metadata-and-xmp).
- Local rotating logs and PLCrashReporter capture are implemented in source;
  no capture/rotation/formatting execution has taken place. The dependency is
  pinned, its license travels with the bundle and reports remain local. See
  [diagnostics design and limits](PHASE9_DEVELOPMENT.md#local-logs-and-crash-reports).
- Named processing versions now have a source implementation (schema 18):
  full-document storage, guarded CRUD, one-step Undo on restore and eight-language
  UI. Qualify round trips, stale revisions/quotas, corruption/failure behavior,
  autosave/close/update coordination, migration and backups. See
  [version behavior](PHASE9_DEVELOPMENT.md#named-processing-versions).
- The §33 History browser now has a source implementation: chronological states,
  arbitrary navigation, guarded clear and named-version creation from any retained
  state. History stays session-local (200 steps); the UI states its lifetime.
  Qualify branch/gesture semantics, stale handles/confirmation, snapshot fidelity,
  keyboard/scroll behavior and autosave/close coordination. Persistent history is
  not implemented. See [History behavior](PHASE9_DEVELOPMENT.md#history-browser).
- The §17 Histogram and §18 Option clipping previews now have source workflows:
  bounded RGB/luminance sampling before display ICC, alpha-weighted counts,
  output-referred clipping overlays and five tone controls with gesture Undo.
  Qualify numerical/visual correctness, stale-render/cache behavior, progressive
  and 1:1 updates, keyboard/Option/focus handling, UI size/languages and cost on
  supported hardware. These are preview estimates, not RAW sensor saturation or
  an export-profile soft proof. See [Histogram behavior](PHASE9_DEVELOPMENT.md#histogram-and-clipping).
- Requirement §18 Auto WB / neutral eyedropper now have source implementations.
  Qualify gray-world scene bias and the shared Temperature/Tint solver against
  independent photographic/numerical references; cover camera calibration,
  alpha/clipping/geometry, source-versus-display semantics, one-step Undo,
  persistence/export, stale-result/cancel/shutdown handling and native UI in all
  languages. The result is Custom WB, not a live Auto mode. See
  [WB behavior and limits](PHASE9_DEVELOPMENT.md#auto-white-balance-and-neutral-eyedropper).

## Eight functional source blocks completed

Presence, Point Color, Color Grading, Post Crop Vignette/Grain, Smart Collections,
Compare, Auto Adjust and advanced import now have source implementations. See
[the ledger](FUNCTIONAL_COMPLETION.md) and [ADR 0030](adr/0030-functional-completion.md).
Their completion does not close any qualification gate. In the final campaign:

- Build all targets and reconcile fixtures with catalog 21, preset codecs 8/9,
  snapshot codec 2, the expanded import dialog and new Library/QML dependencies.
- Qualify zero-setting equivalence, texture/detail halos, haze/color shifts,
  grayscale/transparent/HDR inputs, grading overlap, output-space vignette/grain,
  preview/full-resolution/export agreement and deterministic rendering.
- Cover old/new catalog/preset/snapshot round trips, migration backup/restore,
  creative batch conflicts/Undo, history, cancellation and stale picker/Auto results.
- Cover nested rule semantics, keyword inequality, escaped searches, empty results,
  metadata refresh, rename races, limits, all eight languages and accessibility.
- Cover Compare Fit/100%, different aspect ratios, linked center/zoom, swap,
  unsaved edits, missing/changed sources, close/reopen and memory-pressure behavior.
- Fault-inject advanced import at every journal/copy/backup/commit/Move boundary;
  cover date fallbacks, filename collisions, symlinks, changed sources, full disks,
  metadata/develop snapshots, duplicates, cancellation, retained originals and
  recovery after rename/unlink. Verify both image backups and catalog invariants.

## Final campaign

Use a new candidate branch; the implementation branch intentionally skips CI.
Build all application, CLI and test targets first and fix integration/compiler
issues. Run formatting/static analysis then the existing tests and extended
coverage for every new workflow in phases 0.6–0.9. Preserve historical failure
evidence, update obsolete fixture assumptions explicitly and never reinterpret
SKIP/NOT_RUN/blocked cases as passing.

Build the offline models before configuring the final app. Qualify the pinned
Python/PyTorch/ONNX/Core ML conversion path, source-to-Core-ML numerical agreement,
model tensor contracts and toolchain compatibility. Bundle v4 now passes the
synthetic source-quality, native CPU/ALL and bounded photographic/runtime checks;
cross-platform and complete photographic/runtime qualification remain open.
Use representative photographs to assess denoise texture/color/highlights/tiles
and Sky boundaries/no-sky errors, then CPU fallback, cancellation and 8/16 GB
resource use. Exercise first launch without network, installation in Applications,
app moves, custom override/reset, upgrade/relaunch and both models in the final
signed ZIP/DMG. Do not treat checksum/provenance checks as model-quality evidence.

The complete campaign must include requirements §100, all 23 open 0.5.1
qualification gaps, LUMA-BUG-035, image/CPU–Metal comparisons, trained-model
quality, export/ICC/metadata, migration/rollback/downgrade, fault-injected recovery,
native dialogs/keyboard/focus/sleep/wake, all eight languages, 8/16 GB Apple
Silicon resource tests and 10k/100k/500k catalog/load/stress cases. Record exact
source/build identity, machine/OS, fixtures and per-case evidence. Fix failures
and repeat the affected regression coverage before final acceptance.

Startup recovery and local Diagnostics now have source implementations. Include
missing/locked/corrupt catalogs, interrupted-session detection, worker shutdown,
restore-without-replacement and diagnostic-report privacy in this campaign.
Qualify the separate local crash reporter, signal/exception capture, bounded
binary/text reports, queue overflow, rotation/retention, inactive-session locks,
multiple processes, debugger/acceptance opt-outs and disk/permission failures.
Preserve matching final application/library dSYMs before stripping or removing
build objects and demonstrate offline symbolication against their Mach-O UUIDs.
Crash reports contain binary paths/process details; never automatically append
them to the privacy-limited diagnostic JSON or upload them.

## Distribution

User decision, 2026-10-22: Apple Developer Program is unavailable. A full release
means open-source publication on GitHub, with standalone DMG/ZIP and the signed
Sparkle update channel. Developer ID/notarization are now optional and are no
longer release blockers. This supersedes the previous signing requirement only;
it does not waive the final functional, image, UX, recovery or load qualification.
See [the current distribution policy](OPEN_SOURCE_RELEASE.md).

The existing release workflows and Sparkle signing checks remain in place.
The 0.5.1 limited-release exception does not authorize a 1.0 exception. Final
distribution needs standalone Applications installation/launch, DMG/ZIP checksums, updater signature
verification, bell notification, upgrade/relaunch and data preservation. Apple
Developer signing credentials are neither required nor configured for this release.
The [Developer ID packaging path](DEVELOPER_ID_RELEASE.md) is now implemented
in source, including isolated Actions credential setup and accepted/stapled
app/DMG tickets. This optional path is unexecuted. The selected ad-hoc path must
retain explicit `notarized: false` provenance and be tested with a quarantined
download using the documented per-app macOS approval; no global security change.

The available local Mac has 8 GiB RAM. The user has no 16 GiB Mac and can connect
an external monitor. Neither the 16 GiB configuration nor external-display checks
are marked passed by that availability statement.

Publish only after all required results are documented and accepted. Keep the
post-1.0 Impeccable/Taste frontend redesign outside this qualification scope.
