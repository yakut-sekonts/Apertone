#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_directory="${1:-build}"
./scripts/package.sh "$build_directory"
version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' dist/Luma.app/Contents/Info.plist)
./scripts/package-dmg.sh "$version"
mkdir -p dist/release
cp dist/Luma-macos-arm64.zip "dist/release/Luma-$version-macos-arm64.zip"
cp "dist/Luma-$version-macos-arm64.dmg" dist/release/
cp dist/Luma.app/Contents/Resources/BUILD_INFO.json dist/release/
(cd dist/release && shasum -a 256 "Luma-$version-macos-arm64.zip" "Luma-$version-macos-arm64.dmg" BUILD_INFO.json > SHA256SUMS && shasum -a 256 -c SHA256SUMS)
