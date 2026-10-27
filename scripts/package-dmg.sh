#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
version="${1:?Specify release version}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid release version" >&2; exit 1; }
app="$PWD/dist/Apertone.app"
actual=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist")
[[ "$actual" == "$version" ]] || { echo "Bundle/release version mismatch" >&2; exit 1; }
codesign --verify --deep --strict "$app"
workspace=$(mktemp -d "$PWD/dist/luma-dmg.XXXXXX")
mounted=0
mountpoint="$workspace/volume"
cleanup() {
    if [[ "$mounted" == 1 ]]; then
        hdiutil detach "$mountpoint" >/dev/null 2>&1 ||
            hdiutil detach -force "$mountpoint" >/dev/null 2>&1 || {
                echo "Could not detach generated image; preserving $workspace" >&2
                return
            }
    fi
    rm -rf "$workspace"
}
trap cleanup EXIT
staging="$workspace/contents"
mkdir "$staging"
ditto "$app" "$staging/Apertone.app"
cp "$app/Contents/Resources/Apertone.icns" "$staging/.VolumeIcon.icns"
ln -s /Applications "$staging/Applications"
cp LICENSE "$staging/LICENSE.txt"
cp "docs/releases/v$version.md" "$staging/READ_ME.md"
# Prevent Spotlight from indexing the transient image during creation.
touch "$staging/.metadata_never_index"
# DiskImages can return EBUSY on hosted macOS runners. Retry only that error;
# every attempt has its own output, so a partial image cannot become a release.
# https://github.com/create-dmg/create-dmg/issues/190
for attempt in 1 2 3 4; do
    image="$workspace/attempt-$attempt.dmg"
    if LC_ALL=C hdiutil create -volname "Apertone $version" -srcfolder "$staging" \
        -fs HFS+ -format UDRW "$image" >"$workspace/create.log" 2>&1; then
        cat "$workspace/create.log"
        break
    else
        status=$?
        cat "$workspace/create.log" >&2
        if ! /usr/bin/grep -Fq 'Resource busy' "$workspace/create.log" || [[ "$attempt" == 4 ]]; then
            exit "$status"
        fi
        echo "DiskImages is busy; retrying DMG creation (attempt $((attempt + 1))/4)." >&2
        sleep "$((attempt * 5))"
    fi
done
# The volume root's Finder flags are distinct from the staging directory flags.
# Set them on the mounted writable volume, then compress the completed image.
hdiutil attach -readwrite -nobrowse -noautoopen -mountpoint "$mountpoint" "$image" > "$workspace/attach.log"
mounted=1
xcrun SetFile -a C "$mountpoint"
hdiutil detach "$mountpoint"
mounted=0
hdiutil convert "$image" -format UDZO -o "$workspace/final.dmg"
hdiutil verify "$workspace/final.dmg"
mv -f "$workspace/final.dmg" "$PWD/dist/Apertone-$version-macos-arm64.dmg"
