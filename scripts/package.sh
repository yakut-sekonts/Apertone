#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_directory="${1:-build}"
sdk="$PWD/.tools/qt/6.10.3/macos"
output="$PWD/dist"
mkdir -p "$output"
# Only generated output in dist is replaced; sources and catalogs are untouched.
if [[ -d "$output/Luma.app" ]]; then
    mv "$output/Luma.app" "$output/Luma.previous.$(date +%s).app"
fi
ditto "$build_directory/Luma.app" "$output/Luma.app"
mkdir -p "$output/Luma.app/Contents/Frameworks"
cp "$build_directory/libluma_raw.dylib" "$output/Luma.app/Contents/Frameworks/"
# Exclude Qt's optional PostgreSQL/ODBC drivers and their external dependencies.
# Luma uses the pinned SQLite C API, not those drivers.
"$sdk/bin/macdeployqt" "$output/Luma.app" -qmldir="$PWD/apps/desktop/qml" -always-overwrite -no-strip -appstore-compliant
notices="$output/Luma.app/Contents/Resources/ThirdParty"
mkdir -p "$notices/sources"
cp LICENSE THIRD_PARTY_LICENSES.md "$notices/"
cp "$build_directory/_deps/libraw-src/LICENSE.LGPL" "$notices/LibRaw-LGPL.txt"
cp "$build_directory/_deps/libraw-src/COPYRIGHT" "$notices/LibRaw-COPYRIGHT.txt"
cp "$build_directory/_deps/lcms-src/LICENSE" "$notices/LittleCMS-LICENSE.txt"
test -s "$notices/Sparkle-LICENSE.txt"
# Sources are reused by exact archive/version name after a complete tar validation.
# Interrupted downloads never replace the cached archive.
source_cache="$PWD/.tools/packaging-sources"
mkdir -p "$source_cache"
archive_source() {
    local url="$1" archive="$2"
    if [[ ! -f "$source_cache/$archive" ]]; then
        curl -fL --connect-timeout 15 --max-time 300 --speed-limit 1024 --speed-time 30 --retry 3 \
            "$url" -o "$source_cache/$archive.partial"
        tar -tf "$source_cache/$archive.partial" > /dev/null
        mv "$source_cache/$archive.partial" "$source_cache/$archive"
    fi
    tar -tf "$source_cache/$archive" > /dev/null
    cp "$source_cache/$archive" "$notices/sources/"
}
archive_source https://www.libraw.org/data/LibRaw-0.21.5.tar.gz LibRaw-0.21.5.tar.gz
for module in qtbase qtdeclarative qtsvg; do
    archive="$module-everywhere-src-6.10.3.tar.xz"
    archive_source "https://download.qt.io/archive/qt/6.10/6.10.3/submodules/$archive" "$archive"
    .tools/venv/bin/python scripts/extract-qt-notices.py "$notices/sources/$archive" "$notices/$module"
done
# Optional QML modules cause macdeployqt to copy QtSql drivers, including drivers
# for external DB vendors. Luma uses SQLite's C API and has no QtSql driver usage.
rm -rf "$output/Luma.app/Contents/PlugIns/sqldrivers"
.tools/venv/bin/python - "$output/Luma.app" <<'PYTHON'
import json, plistlib, subprocess, sys
from pathlib import Path
app = Path(sys.argv[1])
with (app / "Contents/Info.plist").open("rb") as file:
    version = plistlib.load(file)["CFBundleShortVersionString"]
commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
info = {"version": version, "source_commit": commit, "platform": "macOS 14+", "architecture": "arm64",
        "signing": "ad-hoc", "notarized": False,
        "dependencies": {"Qt": "6.10.3", "LibRaw": "0.21.5", "SQLite": "3.50.4", "LittleCMS": "2.17", "Sparkle": "2.10.0"}}
(app / "Contents/Resources/BUILD_INFO.json").write_text(json.dumps(info, indent=2) + "\n")
PYTHON
codesign --force --deep --sign - "$output/Luma.app"
codesign --verify --deep --strict "$output/Luma.app"
ditto -c -k --sequesterRsrc --keepParent "$output/Luma.app" "$output/Luma-macos-arm64.zip"
(cd "$output" && shasum -a 256 Luma-macos-arm64.zip > SHA256SUMS)
echo "Packaged: $output/Luma.app"
