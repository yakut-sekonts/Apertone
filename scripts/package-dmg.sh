#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
version="${1:?Specify release version}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid release version" >&2; exit 1; }
app="$PWD/dist/Luma.app"
actual=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist")
[[ "$actual" == "$version" ]] || { echo "Bundle/release version mismatch" >&2; exit 1; }
codesign --verify --deep --strict "$app"
staging=$(mktemp -d "$PWD/dist/luma-dmg.XXXXXX")
trap 'rm -rf "$staging"' EXIT
ditto "$app" "$staging/Luma.app"
ln -s /Applications "$staging/Applications"
cp LICENSE "$staging/LICENSE.txt"
cp "docs/releases/v$version.md" "$staging/READ_ME.md"
hdiutil create -volname "Luma $version" -srcfolder "$staging" -fs HFS+ -format UDZO \
    -ov "$PWD/dist/Luma-$version-macos-arm64.dmg"
hdiutil verify "$PWD/dist/Luma-$version-macos-arm64.dmg"
