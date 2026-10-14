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
    geometry_source = Path(temporary) / "Building RAW.DNG"
    def building(x, y):
        u, v = 2*x/319-1, 2*y/239-1
        a, b, c, d = 0.85-u*0.13, -0.06+u*0.12, 0.04-v*0.13, 0.85+v*0.12
        sx, sy = (u*d-b*v)/(a*d-b*c), (a*v-u*c)/(a*d-b*c)
        edge = (abs(abs(sx)-0.55)<0.014 and abs(sy)<0.8) or (abs(abs(sy)-0.5)<0.014 and abs(sx)<0.8)
        return 0.02 if edge else 0.7
    create_dng(geometry_source, width=320, height=240, sample=building)
    geometry_hash = hashlib.sha256(geometry_source.read_bytes()).hexdigest()
    lens_hash = hashlib.sha256(lens_source.read_bytes()).hexdigest()
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    result = subprocess.run([sys.argv[1], str(source), str(Path(__file__).resolve().parents[2] / "apps/desktop/qml"), str(lens_source), str(geometry_source)], capture_output=True, text=True,
                            env={**os.environ, "QT_QPA_PLATFORM": "offscreen",
                                 "QT_QUICK_BACKEND": "software"}, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr
    assert hashlib.sha256(source.read_bytes()).hexdigest() == original, "Preset test modified original RAW"
    assert hashlib.sha256(lens_source.read_bytes()).hexdigest() == lens_hash
    assert hashlib.sha256(geometry_source.read_bytes()).hexdigest() == geometry_hash
    print(result.stdout.strip())
