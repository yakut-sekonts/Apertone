#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${1:-build/models}"
# This script is deliberately not called during the source-only development
# phase. Release preparation invokes it explicitly before configuring the app.
python3.12 -m venv .tools/model-venv
.tools/model-venv/bin/python -m pip install --disable-pip-version-check -r scripts/model-requirements.txt
.tools/model-venv/bin/python scripts/convert-models.py --output "$output"
