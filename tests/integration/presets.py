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
    lens_source = Path(temporary) / "Lens RAW.DNG"
    create_dng(lens_source, maker="NIKON CORPORATION", model="NIKON D7500",
               lens="AF-S DX Nikkor 18-140mm f/3.5-5.6G ED VR", focal=18, aperture=5.6)
    lens_hash = hashlib.sha256(lens_source.read_bytes()).hexdigest()
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    result = subprocess.run([sys.argv[1], str(source), str(Path(__file__).resolve().parents[2] / "apps/desktop/qml"), str(lens_source)], capture_output=True, text=True,
                            env={**os.environ, "QT_QPA_PLATFORM": "offscreen",
                                 "QT_QUICK_BACKEND": "software"}, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    assert hashlib.sha256(source.read_bytes()).hexdigest() == original, "Preset test modified original RAW"
    assert hashlib.sha256(lens_source.read_bytes()).hexdigest() == lens_hash
    print(result.stdout.strip())
