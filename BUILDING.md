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
archives, produces an ad-hoc-signed `dist/Luma.app`, and checksums `Luma-macos-arm64.zip`.
The app should not depend on the developer's Qt SDK or Homebrew paths at runtime.
Developer ID signing, notarization and DMG releases are separate later work.

## Catalog location

The default location is resolved through Qt's `AppLocalDataLocation` on macOS.
The left panel displays the actual catalog path. Pass `--catalog FILE` to either
executable for a separate catalog. The initial workflow is Add-only: source
files stay in place. Keep their directories available after restart.

Known current constraints: session-local undo only; no grid, named versions or relocating
missing files; no claim of calibrated production image quality or sub-50 ms
preview latency. Current limitations are tracked in the roadmap and changelog.

## Develop acceptance

The develop-export-restart test exercises all parameters through the shared CLI,
real desktop Undo/Redo, independent restarts and byte-identical PNG exports.
It verifies JPEG/PNG ICC information, 16-bit TIFF and full-resolution dimensions,
invalid batch rollback, overwrite policy, original/hard-link protection and
cancelled export cleanup. Core tests include migration from schema 1, parameter
boundaries and monotonic tone processing.

Catalog schema 2 cannot be opened by the older M0 binary. Keep a backup before
using a newer build on an important catalog; migration preserves M0 edits.
