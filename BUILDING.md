# Building Luma on macOS

## Requirements

- Apple Silicon Mac with macOS 14+, 8 GB RAM minimum (16 GB recommended).
- Apple Command Line Tools or Xcode with a C++20 compiler.
- Python 3.12+ and Internet for initial dependency setup.

Bootstrap creates a project-local `.tools` directory; no global installation or
Homebrew runtime library is required. CMake FetchContent is the primary runtime
source-dependency mechanism. Qt is the official version-pinned SDK, dynamically
linked, obtained with a version-pinned aqt installer. Build-tool Python packages
and transitive versions are locked in `scripts/toolchain-requirements.txt`.

```bash
./scripts/bootstrap.sh
export PATH="$PWD/.tools/venv/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open build/Luma.app
```

LibRaw 0.21.5, SQLite 3.50.4 and LittleCMS 2.17 source archives have mandatory
SHA-256 verification in `cmake/Dependencies.cmake`. Qt must be exactly 6.10.3.
FetchContent keeps source files in the build directory for subsequent builds.
The app can be opened without a Developer ID certificate in a local debug build.

## RAW acceptance tests

Public tests generate their own small Bayer DNG. Never copy private photos into
the repository. Set either environment variable to an external directory:

```bash
export LUMA_PRIVATE_TEST_ASSETS="$HOME/LumaTestAssets"
# PHOTON_PRIVATE_TEST_ASSETS is also accepted for compatibility with the supplied specification.
ctest --test-dir build --output-on-failure
```

The Nikon test checks the camera model is D7500. Without an eligible file it is
reported as **skipped**, never as a successful Nikon acceptance run. It checks
metadata, original SHA-256, separate CLI processes and separate desktop processes
with a real QML viewer and the real autosave controller.

## Static analysis

```bash
./scripts/check-format.sh
./scripts/check-tidy.sh build
```

Own C++ targets use `-Wall -Wextra -Wpedantic -Werror`; fetched third-party
sources retain their upstream warnings. CI runs on macOS Apple Silicon only.

## Bundle

```bash
./scripts/package.sh build
```

This bundles the used Qt frameworks/plugins, LibRaw, LGPL notices and source
archives, produces an ad-hoc-signed `dist/Luma.app`, and checksums `Luma-macos-arm64.zip`. Exact-version source archives are cached
locally after tar validation; downloads have connection/transfer timeouts.
Unused QtSql drivers are removed from the bundle.
The app should not depend on the developer's Qt SDK or Homebrew paths at runtime.
Developer ID signing and notarization remain separate work; see Release packaging for DMG.

## Catalog location

The default location is resolved through Qt's `AppLocalDataLocation` on macOS.
The Develop left panel displays the actual catalog path. Pass `--catalog FILE` to either
executable for a separate catalog. Add keeps source
files in place; Copy stores verified copies in the chosen destination. Keep the
registered original/copy directories available after restart.

Known current constraints: session-local undo only; no named versions or relocating
missing files; no claim of calibrated production image quality or sub-50 ms
preview latency. Current limitations are tracked in the roadmap and changelog.

## Develop acceptance

The develop-export-restart test exercises all parameters through the shared CLI,
real desktop Undo/Redo, independent restarts and byte-identical PNG exports.
It verifies JPEG/PNG ICC information, 16-bit TIFF and full-resolution dimensions,
invalid batch rollback, overwrite policy, original/hard-link protection and
cancelled export cleanup. Core tests include migration from schema 1, parameter
boundaries and monotonic tone processing.

Catalog schema 4 cannot be opened by v0.3.0 or older binaries. Keep a backup before
using a newer build on an important catalog; migration preserves M0 edits.

## Library acceptance

`library-import-restart` exercises Add/Copy, folder recursion, per-item failures,
checksum duplicates, cancellation, dead-owner recovery and live-owner exclusion.
Core tests cover migration 2→3, concurrent new-catalog initialization and atomic
bulk metadata/collection rollback. A generated 100k-row catalog exercises the
actual Qt model with its 16-page bound, range/select-all and filters. Public GUI
acceptance generates lazy thumbnails, organizes a collection, opens a RAW in
Develop, saves its edit and closes during queued import. It runs offscreen in CI.
This does not certify large real-photo browsing latency or memory pressure.

The thumbnail cache and SQLite journal are local data. Do not commit them,
private RAW files or copied originals. Copy errors identify the source and reason
in `import_items`; the UI shows summary counts and a details dialog (first 100 failures).

## Release packaging

```bash
./scripts/release-artifacts.sh build
.tools/venv/bin/python scripts/verify-bundle.py
```

Release assets live under `dist/release`: versioned ZIP, DMG, BUILD_INFO.json and
SHA256SUMS. The DMG contains Luma.app, an Applications shortcut, license and notes.
BUILD_INFO.json identifies the source commit and dependencies. The ad-hoc-signed
app is not notarized; follow the installation note in the release documentation.

The macOS Release workflow also runs on `codex/release-*` branches to validate
packaging before tagging. Tags `vX.Y.Z` must point to main history. A read-only
build job tests, analyzes, packages and verifies the bundle without the Qt SDK.
Only the separate publishing job has `contents: write`; it creates a draft,
uploads assets, verifies GitHub SHA-256 digests and publishes the complete release.
An already-published release is never overwritten. Only temporary repository
GITHUB_TOKEN credentials are used by the publisher.

If publication fails after `build-package` succeeds, run Recover verified release
publication with the existing tag and the original tag workflow run ID. The
read-only job requires the exact tag commit in main, a successful original builder
and its unexpired artifact. The publisher resumes the draft by release ID, checks
all server digests and publishes the original assets without moving the tag.
The `codex/release-recovery-0.3.0` push trigger bootstraps recovery of the first
release; subsequent recovery runs use workflow_dispatch. Published releases are
refused rather than overwritten.


Sparkle 2.10.0 is fetched by CMake with a fixed SHA-256 and embedded into the
app bundle. The complete upstream license accompanies packaging. See
[UPDATES.md](docs/UPDATES.md) for signing and publication setup.

`advanced-color-unit` / `advanced-export-restart` cover Curves/HSL, migration,
transaction fault injection, targeted GUI gestures and restart PNG/TIFF exports.
`update-restart-guard` checks saves/export/import gating and retry/badge behavior.
`signed-update-feed` checks real CryptoKit verification against RFC 8032, tampered
archives and private-key stdin handling without a production signing key.
