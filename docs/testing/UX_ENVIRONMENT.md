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
