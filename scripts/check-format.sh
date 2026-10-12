#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
formatter="${LUMA_CLANG_FORMAT:-.tools/venv/bin/clang-format}"
find apps core platform tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.mm' \) \
    -print0 | xargs -0 "$formatter" --dry-run --Werror
