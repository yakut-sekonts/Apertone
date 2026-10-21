# Final 1.0 readiness — NOT QUALIFIED

The human postponed all builds, tests, static analysis and QA to final preparation.
This document records the handoff requirements; it is not a test report, waiver
or permission to publish. The stable release remains 0.5.1. Do not tag 1.0.0 or
update its Sparkle channel until the complete campaign, fixes and retests finish.

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

### Completed baseline and current blockers

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

## Source decisions and qualification requirements

- Denoise/Sky delivery is resolved: include both models in the installer and
  Sparkle archive. Pinned NAFNet SIDD and U²-Net Sky sources/licenses, conversion,
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
model tensor contracts and toolchain compatibility. Both models now compile, but
the native CPU/source numerical comparison blocks acceptance of Denoise.
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

The existing release workflows and Sparkle signing checks remain in place.
The 0.5.1 limited-release exception does not authorize a 1.0 exception. Final
distribution also needs Developer ID signing, hardened runtime and notarization,
standalone Applications installation/launch, DMG/ZIP checksums, updater signature
verification, bell notification, upgrade/relaunch and data preservation. Apple
Developer signing credentials have not been supplied or inspected by this work.
The [Developer ID packaging path](DEVELOPER_ID_RELEASE.md) is now implemented
in source, including isolated Actions credential setup and accepted/stapled
app/DMG tickets. It is unexecuted; final signing and installation evidence is
still required. The source guards refuse an ad-hoc 1.x publication.

Publish only after all required results are documented and accepted. Keep the
post-1.0 Impeccable/Taste frontend redesign outside this qualification scope.
