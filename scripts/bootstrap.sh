#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    echo "Luma requires macOS Apple Silicon." >&2
    exit 1
fi
python3 -m venv .tools/venv
.tools/venv/bin/python -m pip install --disable-pip-version-check -r scripts/toolchain-requirements.txt
if [[ ! -f .tools/qt/6.10.3/macos/lib/cmake/Qt6/Qt6Config.cmake ]]; then
    .tools/venv/bin/aqt install-qt mac desktop 6.10.3 clang_64 \
        --outputdir .tools/qt --archives qtbase qtdeclarative qttools qtsvg
fi
echo 'Ready. Add .tools/venv/bin to PATH, then run cmake -S . -B build.'
