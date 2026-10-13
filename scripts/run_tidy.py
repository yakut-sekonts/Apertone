import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = (root / sys.argv[1]).resolve()
commands = json.loads((build / "compile_commands.json").read_text())
failed = False
checked_sources = set()
for entry in commands:
    source = Path(entry["file"])
    try:
        relative = source.relative_to(root)
    except ValueError:
        continue
    if relative.parts[0] not in {"apps", "core", "platform", "tests"}:
        continue
    # clang-tidy processes every compile command for this source in one call.
    if source in checked_sources:
        continue
    checked_sources.add(source)
    result = subprocess.run([str(root / ".tools/venv/bin/clang-tidy"), "-p", str(build), str(source)], cwd=root)
    failed |= result.returncode != 0
raise SystemExit(1 if failed else 0)
