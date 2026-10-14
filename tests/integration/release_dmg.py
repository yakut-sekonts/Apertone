"""Exercise DMG failure recovery without mounting images or changing host settings."""
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest


class DmgRecoveryTests(unittest.TestCase):
    def run_packager(self, failure, busy_attempts=0):
        with tempfile.TemporaryDirectory(prefix="luma-dmg-test-") as temporary:
            root = Path(temporary)
            (root / "scripts").mkdir()
            shutil.copyfile(Path(__file__).resolve().parents[2] / "scripts/package-dmg.sh",
                            root / "scripts/package-dmg.sh")
            contents = root / "dist/Luma.app/Contents"
            contents.mkdir(parents=True)
            with (contents / "Info.plist").open("wb") as file:
                plistlib.dump({"CFBundleShortVersionString": "0.3.0"}, file)
            (contents / "Resources").mkdir()
            (contents / "Resources/Luma.icns").write_bytes(b"synthetic icon")
            (root / "docs/releases").mkdir(parents=True)
            (root / "docs/releases/v0.3.0.md").write_text("Release notes")
            (root / "LICENSE").write_text("License")
            output = root / "dist/Luma-0.3.0-macos-arm64.dmg"
            output.write_bytes(b"previous verified image")
            binaries = root / "bin"
            binaries.mkdir()
            for name in ("codesign", "sleep", "xcrun"):
                command = binaries / name
                command.write_text("#!/bin/sh\nexit 0\n")
                command.chmod(0o755)
            command = binaries / "hdiutil"
            command.write_text('''#!/usr/bin/env python3
import os
from pathlib import Path
import sys
counter = Path(os.environ["DMG_TEST_COUNTER"])
if sys.argv[1] in ("attach", "detach"):
    sys.exit(0)
if sys.argv[1] == "convert":
    Path(sys.argv[-1]).write_bytes(b"new image")
    sys.exit(0)
if sys.argv[1] == "verify":
    sys.exit(3 if os.environ["DMG_TEST_FAILURE"] == "verify" else 0)
staging = Path(sys.argv[sys.argv.index("-srcfolder") + 1])
assert (staging / ".VolumeIcon.icns").read_bytes() == b"synthetic icon"
assert (staging / "Applications").is_symlink()
attempt = int(counter.read_text()) + 1 if counter.exists() else 1
counter.write_text(str(attempt))
Path(sys.argv[-1]).write_bytes(b"new image")
if os.environ["DMG_TEST_FAILURE"] == "other":
    print("hdiutil: create failed - Permission denied", file=sys.stderr)
    sys.exit(7)
if attempt <= int(os.environ["DMG_TEST_BUSY_ATTEMPTS"]):
    print("hdiutil: create failed - Resource busy", file=sys.stderr)
    sys.exit(1)
''')
            command.chmod(0o755)
            env = os.environ.copy()
            env.update(PATH=f"{binaries}:{Path(sys.executable).parent}:{env['PATH']}",
                       DMG_TEST_COUNTER=str(root / "attempts"),
                       DMG_TEST_FAILURE=failure,
                       DMG_TEST_BUSY_ATTEMPTS=str(busy_attempts))
            result = subprocess.run(["/bin/bash", str(root / "scripts/package-dmg.sh"), "0.3.0"],
                                    env=env, capture_output=True, text=True, timeout=30)
            self.assertFalse(list((root / "dist").glob("luma-dmg.*")))
            return result.returncode, int((root / "attempts").read_text()), output.read_bytes()

    def test_busy_recovers_before_replacing_previous_image(self):
        self.assertEqual(self.run_packager("", busy_attempts=2), (0, 3, b"new image"))

    def test_other_error_fails_immediately_and_preserves_previous_image(self):
        self.assertEqual(self.run_packager("other"), (7, 1, b"previous verified image"))

    def test_busy_is_bounded_and_preserves_previous_image(self):
        self.assertEqual(self.run_packager("", busy_attempts=10), (1, 4, b"previous verified image"))

    def test_verification_failure_preserves_previous_image(self):
        self.assertEqual(self.run_packager("verify"), (3, 1, b"previous verified image"))


if __name__ == "__main__":
    unittest.main()
