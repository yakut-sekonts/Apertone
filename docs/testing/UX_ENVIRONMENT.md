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
