# Native UX regression

This separate Qt Test executable loads production Main.qml and the real controllers.
It sends real clicks, drags, keys and ordinary wheel events. A native file-panel
selection may be prepared as a fixture boundary; this does not qualify the native
file chooser itself. It never opens private photographs or the user's real catalog.

Current schedule (user direction, 2026-10-19): until the pre-release campaign
for 1.0.0, run functional/image-quality/UX/data-integrity checks only. Do not run
stress, soak, chaos, performance or large-catalog/repeated-load workflows.
Existing load adapters and historical results are retained for 1.0.0; deferral
never means PASS. Use `--quality-only` for the reviewed functional policy. It keeps load deferrals
visible as SKIPPED and rejects new, unreviewed adapters. Fix and recheck failures
before publishing an intermediate release.

Build with the pinned toolchain:

```sh
.tools/venv/bin/python tests/ux/build_ux.py --build build-verify
.tools/venv/bin/cmake --build build-verify --target luma_ux_metric_tests --parallel 3
.tools/venv/bin/ctest --test-dir build-verify --output-on-failure
.tools/venv/bin/python tests/ux/run_ux_suite.py --quality-only --case UX-MAC-007 --case UX-SEL-002 --runs 3 --require-build-provenance
```

All native UI runs share an exclusive screen/focus lock. Do not interact with the
test window or run other native UI suites concurrently. Use a focused selection
while investigating a reproducer:

```sh
.tools/venv/bin/python tests/ux/run_ux_suite.py --quality-only --case UX-MAC-008 --runs 3
```

The partial selected-photo keyboard regressions (Return and keypad Enter) share
one generated PNG and use separate fresh processes in their own report domain.
Both run automatically in both workflows; their results never qualify
KEY015 or alter the 770-ID specification inventory. Its wrapper defines the bounded
workload directly, so the official-inventory `--quality-only` flag is unnecessary.

```sh
.tools/venv/bin/python tests/ux/run_keyboard_regression.py --build build-verify --runs 3 --require-build-provenance
```

Crop-related gradient handle regressions also use a separate one-PNG supplemental
domain. Normal and thin-photo fixtures exercise each Linear/Radial handle across
four distinct H/V flip states, with exact document/history/pixel/persistence checks.
The thin fixture also rejects ordinary brush, range and new-gradient input in
expanded photo padding. These bounded checks run in CI but do not register or
qualify the full MSK023 scenario; other geometry and projective edge cases remain
separate qualification work.

```sh
.tools/venv/bin/python tests/ux/run_mask_handle_regression.py --build build-verify --runs 3 --require-build-provenance
```

The legacy full-matrix --release-qualification command includes load workloads;
reserve it for the pre-1.0.0 campaign. Its strict full-matrix status is separate
from the requested intermediate functional-quality checkpoint.

Full-matrix qualification repeats blocking scenarios three times and runs other scenarios
once by default. An explicit --runs value applies to every selected scenario.

The complete 770-ID specification inventory is separate from executable adapters.
An implemented product capability without a verified adapter is explicitly SKIPPED;
it cannot satisfy a blocking gate. Absent future/conditional capabilities are
NOT_IMPLEMENTED. Selecting cases, a single phase, or only blocking mode never qualifies a full release.
The default scope follows the CMake source version; qualification cannot exclude it. An incomplete full
run returns nonzero, even if every executed test passes.

Evidence includes result/metrics JSON, UI actions, app log, screenshots while a
window exists, and an isolated SQLite snapshot on failure. Original SHA-256 values
are checked before the suite, after each executed scenario and at suite end.
Source commit, dirty state, a suite-end source snapshot and test-binary SHA are
recorded. Keep the tested sources unchanged when collecting release evidence. The build wrapper
serializes against UI runs and records source/binary hashes after a successful build.
Full release qualification requires this matching record and unchanged source/binary
hashes across the suite. A focused run may still report useful case results while
explicitly recording missing build provenance.

The parent samples a UI heartbeat and kills only its own process group on a >5 s
hard hang. A delayed heartbeat also captures a native stack sample when available.
Restart tests launch distinct processes over shared test state. Persistence fault
tests use a test-only SQLite VFS to return SQLITE_FULL or pause an actual WAL write;
intentional SIGKILL checkpoints are explicitly whitelisted and recorded.

Performance is MEASURE_ONLY on a recorded local Mac. Dispatch timing is not visual
acknowledgment. Frame intervals currently include idle time and must not be used as
an interaction FPS baseline. Trackpad/momentum, soak, the general Chaos Monkey
and tests of packaged app bundles still need executable adapters. Unsupported
--app and unsupported chaos selections are rejected, never silently ignored.
Reports keep this work separate from missing product capabilities.

Deferred until the pre-1.0.0 campaign: scoped history chaos supports only UX-UND-017: Contrast, Shadows and Vibrance Sliders,
discrete Left/Right inputs and Undo/Redo, with an independent full-document hash
and history model. Other CHAOS IDs, pointer gestures and randomized worker delays
are not covered. The default remains exactly 10,000 semantic actions; setup,
Tab traversal and observation inputs do not consume that count.

```sh
.tools/venv/bin/python tests/ux/run_ux_suite.py --suite chaos --case UX-UND-017 --actions 10000 --seed 123456 --require-build-provenance
.tools/venv/bin/python tests/ux/run_ux_suite.py --replay /path/to/saved/replay.jsonl --require-build-provenance
```

The complete seeded plan and execution intent/results are fsynced. Replay is
regenerated byte-for-byte before creating Context or sending UI input. Seed,
count, schema/model/generator, app/Qt version and generated fixture hashes must
match; conflicting flags and corrupt records abort. A shorter explicit count is
smoke evidence, not the full 10,000-action baseline. Full release qualification
refuses that reduction.

The macOS and release workflows run reviewed functional blocking adapters three
times with `--quality-only`. A versioned policy records reviewed bounded adapters
and source-based load deferrals; a new adapter requires explicit policy review.
Quality fixtures use a separate small directory and do not create the 24 MP image.
Deferred tests retain their original inventory mode and do not count as PASS.
This hosted functional gate is not full release qualification. The original
100,000-row Library model contract is opt-in through
`-DLUMA_ENABLE_SCALE_CONTRACT_TESTS=ON` for the pre-1.0.0 campaign; the default
256-row fixture retains ordinary selection, filtering and metadata contracts.

Updater tests use the public native SDK installation-callback boundary with a
non-installing probe, real controller readiness and real QML operations. They do
not download/install Sparkle packages. The Cmd+O scenario checks chooser opening
in Library and Develop; direct reject is fixture cleanup, not native cancel-input
or file-selection qualification. Copy crash recovery uses an intentional process
kill after publishing a complete file but before its catalog commit, followed by
the real LibraryModel startup recovery in a distinct process.
