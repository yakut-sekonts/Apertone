"""Verify the actual read-only installer, its portable branding and exact app version."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
image = Path(sys.argv[1]).resolve()
version = image.name.removeprefix("Luma-").removesuffix("-macos-arm64.dmg")

def run(*args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True,
                          text=True, timeout=120, **kwargs)

with tempfile.TemporaryDirectory(prefix="luma-installer-") as temporary:
    directory = Path(temporary)
    mountpoint = directory / "volume"
    run("hdiutil", "verify", image)
    run("hdiutil", "attach", "-readonly", "-nobrowse", "-noautoopen", "-mountpoint", mountpoint, image)
    try:
        app = mountpoint / "Luma.app"
        with (app / "Contents/Info.plist").open("rb") as file:
            info = plistlib.load(file)
        assert info["CFBundleShortVersionString"] == info["CFBundleVersion"] == version
        assert info["CFBundleIconFile"] == "Luma.icns"
        icon = app / "Contents/Resources/Luma.icns"
        assert icon.read_bytes() == (root / "resources/branding/Luma.icns").read_bytes()
        assert (mountpoint / ".VolumeIcon.icns").read_bytes() == icon.read_bytes()
        assert "C" in run("xcrun", "GetFileInfo", "-a", mountpoint).stdout.strip()
        assert (mountpoint / "Applications").is_symlink()
        assert os.readlink(mountpoint / "Applications") == "/Applications"
        assert (mountpoint / "READ_ME.md").read_bytes() == (root / f"docs/releases/v{version}.md").read_bytes()
        provenance = json.loads((app / "Contents/Resources/BUILD_INFO.json").read_text())
        assert provenance["version"] == version
        assert provenance["source_commit"] == run("git", "rev-parse", "HEAD", cwd=root).stdout.strip()
        run("codesign", "--verify", "--deep", "--strict", app)
        executable = app / "Contents/MacOS/Luma"
        env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}
        assert run(executable, "--version", env=env).stdout.strip() == f"Luma {version}"
        run(executable, "--smoke-test", "--catalog", directory / "catalog.sqlite", env=env)
        print(f"DMG {version}: read-only mount, custom volume/app icons, Applications link, notes, source commit, strict signature and actual binary version/startup passed")
    finally:
        run("hdiutil", "detach", mountpoint)
