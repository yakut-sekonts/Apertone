"""Verify generated public workflows with the developer Qt SDK unavailable."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "tests/assets"))
from generate_dng import create_dng

sdk = root / ".tools/qt/6.10.3/macos"
hidden = sdk.with_name("macos.luma-bundle-verification")
app = root / "dist/Luma.app"
with (app / "Contents/Info.plist").open("rb") as file:
    version = plistlib.load(file)["CFBundleShortVersionString"]
info = json.loads((app / "Contents/Resources/BUILD_INFO.json").read_text())
assert info["version"] == version
assert info["source_commit"] == subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
assert sdk.is_dir() and not hidden.exists()
env = os.environ.copy()
for name in ("QT_QPA_PLATFORM", "QT_QUICK_BACKEND", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
    env.pop(name, None)
env["QT_QUICK_BACKEND"] = "software"
executable = app / "Contents/MacOS/Luma"
sdk.rename(hidden)
try:
    with tempfile.TemporaryDirectory(prefix="luma-bundle-") as temporary:
        folder = Path(temporary)
        sources = folder / "sources"
        sources.mkdir()
        raw = sources / "first.DNG"
        create_dng(raw)
        other = bytearray(raw.read_bytes())
        other[-2:] = b"\0\0"
        (sources / "second.DNG").write_bytes(other)
        checksum = hashlib.sha256(raw.read_bytes()).hexdigest()
        catalog = folder / "catalog.sqlite"
        for arguments in (
            ["--test-library", str(sources)],
            ["--test-edit", str(raw), "--test-exposure", "0.65", "--test-develop", "--test-export", str(folder / "first.png")],
            ["--test-restore", "0.65", "--test-develop", "--test-export", str(folder / "second.png")],
        ):
            result = subprocess.run([str(executable), "--catalog", str(catalog), *arguments], env=env, capture_output=True, text=True, timeout=120)
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            assert "ReferenceError" not in result.stderr and "Unable to assign" not in result.stderr, result.stderr
        assert (folder / "first.png").read_bytes() == (folder / "second.png").read_bytes()
        assert hashlib.sha256(raw.read_bytes()).hexdigest() == checksum
        assert len(list((folder / "catalog.sqlite.luma-cache/thumbnails").glob("*.jpg"))) == 2
        print(f"Standalone Luma {version}: Library, Develop, export/restart and source integrity passed without Qt SDK")
finally:
    hidden.rename(sdk)
