# Final 1.0 readiness — NOT QUALIFIED

The human postponed all builds, tests, static analysis and QA to final preparation.
This document records the handoff requirements; it is not a test report, waiver
or permission to publish. The stable release remains 0.5.1. Do not tag 1.0.0 or
update its Sparkle channel until the complete campaign, fixes and retests finish.

## Source decisions and qualification requirements

- Denoise/Sky delivery is resolved: include both models in the installer and
  Sparkle archive. Pinned NAFNet SIDD and U²-Net Sky sources/licenses, conversion,
  default model lookup and packaging guards are implemented in source. The
  compiled models and installer have not been built. See [model delivery](BUNDLED_MODELS.md).
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
model tensor contracts and toolchain compatibility; no conversion has run yet.
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
