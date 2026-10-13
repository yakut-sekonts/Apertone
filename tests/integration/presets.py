"""Run preset acceptance with a generated RAW; user catalog and photos are untouched."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
from generate_dng import create_dng

with tempfile.TemporaryDirectory(prefix="luma-presets-") as temporary:
    source = Path(temporary) / "Preset RAW.DNG"
    create_dng(source)
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    result = subprocess.run([sys.argv[1], str(source), str(Path(__file__).resolve().parents[2] / "apps/desktop/qml")], capture_output=True, text=True,
                            env={**os.environ, "QT_QPA_PLATFORM": "offscreen",
                                 "QT_QUICK_BACKEND": "software"}, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    assert hashlib.sha256(source.read_bytes()).hexdigest() == original, "Preset test modified original RAW"
    print(result.stdout.strip())
