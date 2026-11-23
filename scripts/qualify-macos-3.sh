#!/bin/bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    echo "Run this check in a native Apple Silicon macOS shell." >&2
    exit 1
fi
if (( $# != 1 && $# != 3 )); then
    echo "Usage: bash scripts/qualify-macos-3.sh COMMIT [/absolute/photo.NEF /absolute/photo.jpg]" >&2
    exit 2
fi

commit="$1"
shift
cd "$(dirname "$0")/.."
if [[ ! "$commit" =~ ^[0-9a-f]{40}$ ]] || [[ "$(git rev-parse HEAD)" != "$commit" ]]; then
    echo "Check out the expected full commit in a dedicated Apertone checkout." >&2
    exit 1
fi
if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "The tracked source must match the commit being checked." >&2
    exit 1
fi
grep -Eq '^project\(Apertone VERSION 3\.0\.0 ' CMakeLists.txt
for photo in "$@"; do
    if [[ "$photo" != /* || ! -f "$photo" ]]; then
        echo "Use absolute paths to the two existing test photos." >&2
        exit 2
    fi
done

evidence=$(mktemp -d "${TMPDIR:-/tmp}/apertone-3-macos.XXXXXXXX")
echo "Apertone source: $commit"
echo "Local evidence: $evidence"
exec > >(tee "$evidence/qualification.log") 2>&1

./scripts/bootstrap.sh
bash scripts/build-bundled-models.sh build/models
tools="$PWD/.tools/venv/bin"
"$tools/cmake" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_MAKE_PROGRAM=$tools/ninja"
"$tools/cmake" --build build --parallel 3
"$tools/ctest" --test-dir build --output-on-failure --label-exclude load
"$tools/python" tests/ux/build_ux.py --build build
"$tools/python" tests/ux/run_keyboard_regression.py --build build --runs 3 --require-build-provenance
./scripts/release-artifacts.sh build
"$tools/python" scripts/verify-bundle.py
"$tools/python" scripts/verify-dmg.py dist/release/Apertone-3.0.0-macos-arm64.dmg

if (( $# == 2 )); then
    cli="$PWD/build/bin/apertone"
    shasum -a 256 "$@" > "$evidence/originals.sha256"
    "$cli" import add "$@" --catalog "$evidence/photos.sqlite"
    index=0
    for photo in "$@"; do
        index=$((index + 1))
        "$cli" open "$photo" --catalog "$evidence/photos.sqlite"
        "$cli" exposure 0.35 --catalog "$evidence/photos.sqlite"
        "$cli" export "$evidence/photo-$index.jpg" --catalog "$evidence/photos.sqlite"
        test -s "$evidence/photo-$index.jpg"
    done
    "$cli" catalog verify --catalog "$evidence/photos.sqlite"
    shasum -a 256 -c "$evidence/originals.sha256"
    printf 'Open the isolated catalog for a native UI pass:\n  open -n %q --args --catalog %q\n' \
        "$PWD/dist/Apertone.app" "$evidence/photos.sqlite"
else
    echo "Private photo check skipped because no NEF and JPEG were supplied."
fi
echo "Local macOS qualification passed for $commit."
