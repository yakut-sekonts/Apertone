#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_directory="${1:-build}"
./scripts/package.sh "$build_directory"
version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' dist/Apertone.app/Contents/Info.plist)
./scripts/package-dmg.sh "$version"
if [[ -n "${LUMA_SIGNING_IDENTITY:-}" ]]; then
    .tools/venv/bin/python scripts/sign-app.py dmg "dist/Apertone-$version-macos-arm64.dmg"
fi
mkdir -p dist/release
cp dist/Apertone-macos-arm64.zip "dist/release/Apertone-$version-macos-arm64.zip"
cp "dist/Apertone-$version-macos-arm64.dmg" dist/release/
cp dist/Apertone.app/Contents/Resources/BUILD_INFO.json dist/release/
if [[ -n "${LUMA_SIGNING_IDENTITY:-}" ]]; then
    .tools/venv/bin/python - <<'PYTHON'
import json
from pathlib import Path
path = Path("dist/release/BUILD_INFO.json")
info = json.loads(path.read_text())
receipt = json.loads(Path("dist/notarization.json").read_text())
if any(info[key] != receipt[key] for key in ("version", "source_commit")) or any(receipt.get(kind, {}).get("status") != "Accepted" for kind in ("app", "dmg")):
    raise SystemExit("Missing notarization acceptance for this app/DMG pair.")
info["notarized"] = True
info["notarization"] = {kind: receipt[kind] for kind in ("app", "dmg")}
path.write_text(json.dumps(info, indent=2) + "\n")
PYTHON
fi
(cd dist/release && shasum -a 256 "Apertone-$version-macos-arm64.zip" "Apertone-$version-macos-arm64.dmg" BUILD_INFO.json > SHA256SUMS && shasum -a 256 -c SHA256SUMS)
