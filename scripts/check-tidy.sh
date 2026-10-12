#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_directory="${1:-build}"
.tools/venv/bin/python scripts/run_tidy.py "$build_directory"
