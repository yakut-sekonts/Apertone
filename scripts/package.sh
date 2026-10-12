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
"$sdk/bin/macdeployqt" "$output/Luma.app" -qmldir="$PWD/apps/desktop/qml" -always-overwrite -no-strip
notices="$output/Luma.app/Contents/Resources/ThirdParty"
mkdir -p "$notices/sources"
cp LICENSE THIRD_PARTY_LICENSES.md "$notices/"
cp "$build_directory/_deps/libraw-src/LICENSE.LGPL" "$notices/LibRaw-LGPL.txt"
cp "$build_directory/_deps/libraw-src/COPYRIGHT" "$notices/LibRaw-COPYRIGHT.txt"
cp "$build_directory/_deps/lcms-src/LICENSE" "$notices/LittleCMS-LICENSE.txt"
# Keep exact corresponding LGPL source archives with the distributed bundle.
curl -fL --retry 3 https://www.libraw.org/data/LibRaw-0.21.5.tar.gz -o "$notices/sources/LibRaw-0.21.5.tar.gz"
for module in qtbase qtdeclarative qtsvg; do
    archive="$module-everywhere-src-6.10.3.tar.xz"
    curl -fL --retry 3 "https://download.qt.io/archive/qt/6.10/6.10.3/submodules/$archive" -o "$notices/sources/$archive"
done
codesign --force --deep --sign - "$output/Luma.app"
codesign --verify --deep --strict "$output/Luma.app"
ditto -c -k --sequesterRsrc --keepParent "$output/Luma.app" "$output/Luma-macos-arm64.zip"
(cd "$output" && shasum -a 256 Luma-macos-arm64.zip > SHA256SUMS)
echo "Packaged: $output/Luma.app"
