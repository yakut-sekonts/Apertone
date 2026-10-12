# Luma

Open-source non-destructive RAW photo editor for macOS.

Luma is a local-first photo workspace for Apple Silicon, built with C++20,
Qt 6/QML, LibRaw, SQLite and LittleCMS. No account or cloud service is required.
The product requirements originated under the working name Photon; Luma is the
project name used for implementation.

## Current milestone: M0 — RAW Vertical Slice

- ✅ CMake project and Qt/QML application shell.
- ✅ Real sensor decode through LibRaw, with a project-owned floating-point Bayer pipeline.
- ✅ As Shot white balance, bilinear demosaic and camera-to-linear-sRGB transform.
- ✅ CPU Exposure adjustment from −5 to +5 EV; original comparison.
- ✅ Camera, lens information, ISO, shutter, aperture and focal length.
- ✅ SQLite catalog with WAL, foreign keys, migration, transactions and durable autosave.
- ✅ Automatic restoration of the active photo and Exposure after restart.
- ✅ Fit viewer, resizable panels, drag-and-drop and cancellable decoding.
- ✅ ColorSync display profile → LittleCMS display transform; rerender on screen change.
- ✅ Shared core used by desktop, CLI and tests.
- 🚧 M0 acceptance verification and public CI.
- 🗓 Full Develop, library grid, export, masks, presets and Metal image processing.

Supported platform: **macOS 14+ / Apple Silicon / arm64**. Intel, Windows and
Linux are outside the current scope.

### Image support

M0 implements **integer Bayer RAW** only, including Nikon NEF and Bayer DNG.
LibRaw can decode other Bayer camera formats, but those are not claimed as
verified until they have acceptance coverage. X-Trans, monochrome RAW,
floating-point/RGB DNG and JPEG/HEIF/TIFF ingestion are planned.
Develop never uses the embedded JPEG as its image source.

This is a foundation milestone, not a daily-use Lightroom replacement.
Bilinear demosaic is a reference implementation; advanced demosaic, highlight
reconstruction, noise reduction and production camera profiles remain planned.

## Build and run

```bash
git clone https://github.com/yakut-sekonts/Luma.git
cd Luma
./scripts/bootstrap.sh
export PATH="$PWD/.tools/venv/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
open build/Luma.app
```

See [BUILDING.md](BUILDING.md) for dependency setup, packaging and private RAW tests.
Use `--catalog /path/to/catalog.sqlite` to select an isolated catalog. Without
this option, the app uses its macOS application-data directory. RAW files stay
at their original paths. The catalog contains absolute paths and editing data;
it is local user data and must not be committed.

```bash
build/bin/luma open /path/to/photo.NEF --catalog /path/to/catalog.sqlite
build/bin/luma exposure 1.25 --catalog /path/to/catalog.sqlite
build/bin/luma restore --catalog /path/to/catalog.sqlite
build/bin/luma catalog verify --catalog /path/to/catalog.sqlite
```

## Development

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) ·
[Requirements](docs/REQUIREMENTS.md) · [Contributing](CONTRIBUTING.md)

Only project-generated test images and code may be published. Private RAW
acceptance assets are external and are skipped when not supplied.

Licensed under [Apache-2.0](LICENSE). Third-party components retain their own
licenses; see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
