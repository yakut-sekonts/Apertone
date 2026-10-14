"""Exercise the real application in every supported locale with isolated state."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="luma-languages-") as temporary:
    for code in ("en", "ru", "zh_CN", "hy", "de", "es", "it", "fr"):
        catalog = Path(temporary) / code / "catalog.sqlite"
        result = subprocess.run([sys.argv[1], "--smoke-test", "--test-language", code, "--catalog", str(catalog)], capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}, timeout=30)
        assert result.returncode == 0, result.stdout + result.stderr
        for error in ("ReferenceError", "TypeError", "Unable to assign", "Cannot assign", "Binding loop"):
            assert error not in result.stderr, (code, result.stderr)
        assert json.loads((catalog.parent / "language.json").read_text())["language"] == code
    print("Native Main/Preferences runtime retranslation passed in all eight languages")
