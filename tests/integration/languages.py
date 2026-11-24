"""Exercise the real application in every supported locale with isolated state."""
import itertools
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="luma-languages-") as temporary:
    for width, code in itertools.product((1000, 1320), ("en", "ru", "zh_CN", "hy", "de", "es", "it", "fr")):
        catalog = Path(temporary) / f"{code}-{width}" / "catalog.sqlite"
        result = subprocess.run([sys.argv[1], "--smoke-test", "--test-window-width", str(width), "--test-language", code, "--catalog", str(catalog)], capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}, timeout=30)
        assert result.returncode == 0, (code, width, result.stdout + result.stderr)
        for error in ("ReferenceError", "TypeError", "Unable to assign", "Cannot assign", "Binding loop"):
            assert error not in result.stderr, (code, result.stderr)
        assert json.loads((catalog.parent / "language.json").read_text())["language"] == code
    print("Native Main/Preferences runtime retranslation passed in all eight languages at minimum and default window widths")

    # At 200% this display provides a 900 × 585 logical workspace. Exercise the
    # normal initial size without the window-width override used above.
    root = Path(temporary)
    screen = root / "screen.json"
    screen.write_text(json.dumps({"screens": [{"name": "compact", "width": 1800, "height": 1170,
                                                "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}))
    image = root / "scaled.png"
    result = subprocess.run([sys.argv[1], "--smoke-test", "--catalog", str(root / "scaled.sqlite"),
                             "--test-screenshot", str(image)], capture_output=True, text=True, timeout=30,
                            env={**os.environ, "QT_QPA_PLATFORM": f"offscreen:configfile={screen}",
                                 "QT_QUICK_BACKEND": "software", "QT_SCALE_FACTOR": "2"})
    assert result.returncode == 0 and "Binding loop" not in result.stderr, result.stderr
    with image.open("rb") as png:
        pixel_width, pixel_height = struct.unpack_from(">II", png.read(24), 16)
    assert 0 < pixel_width <= 1800 and 0 < pixel_height <= 1170, (pixel_width, pixel_height)
