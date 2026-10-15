"""Feed validation, secret handling and real CryptoKit signature checks; no network."""
import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("appcast", ROOT / "scripts/create-appcast.py")
appcast = importlib.util.module_from_spec(spec)
spec.loader.exec_module(appcast)
# RFC 8032 Ed25519 test vector 1: an empty message. Public test data, no app key.
PUBLIC = base64.b64encode(bytes.fromhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")).decode()
SIGNATURE = base64.b64encode(bytes.fromhex(
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
    "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b")).decode()

class AppcastTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luma-appcast-test-")
        self.root = Path(self.temporary.name)
        (self.root / "platform/macos").mkdir(parents=True)
        (self.root / "scripts").mkdir()
        shutil.copy(ROOT / "scripts/verify-update-signature.swift", self.root / "scripts")
        with (self.root / "platform/macos/Info.plist.in").open("wb") as file:
            plistlib.dump({"SUPublicEDKey": PUBLIC}, file)
        self.archive = self.root / "Luma-0.4.0-macos-arm64.zip"
        self.archive.write_bytes(b"")
        (self.root / "Luma-0.4.0-macos-arm64.dmg").write_bytes(b"fixture")
        (self.root / "BUILD_INFO.json").write_text(json.dumps({"version": "0.4.0", "source_commit": "a" * 40}))
        self.manifest()
        self.calls = []
        self.real_run = subprocess.run
    def tearDown(self):
        self.temporary.cleanup()
    def manifest(self):
        paths = [self.archive, self.root / "Luma-0.4.0-macos-arm64.dmg", self.root / "BUILD_INFO.json"]
        (self.root / "SHA256SUMS").write_text("".join(
            f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n" for path in paths))
    def run_command(self, command, **kwargs):
        self.calls.append((command, kwargs))
        if command[0] == "test-signer":
            return SimpleNamespace(returncode=0, stdout=SIGNATURE)
        return self.real_run(command, **kwargs)
    def create(self):
        with patch.object(appcast, "ROOT", self.root), patch.object(appcast.subprocess, "run", self.run_command):
            appcast.create(self.root, Path("test-signer"))
    def test_valid_signature_and_matching_release_feed(self):
        self.create()
        item = ET.parse(self.root / "appcast.xml").find("./channel/item")
        enclosure = item.find("enclosure")
        self.assertEqual(enclosure.get(f"{{{appcast.NAMESPACE}}}edSignature"), SIGNATURE)
        self.assertEqual(enclosure.get("length"), "0")
        self.assertEqual(enclosure.get("url"), "https://github.com/yakut-sekonts/Luma/releases/download/v0.4.0/Luma-0.4.0-macos-arm64.zip")
        self.assertIn("appcast.xml", (self.root / "SHA256SUMS").read_text())
    def test_tampered_archive_rejected_before_signing(self):
        self.archive.write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.create()
        self.assertFalse(self.calls)
        self.assertFalse((self.root / "appcast.xml").exists())
    def test_wrong_signature_rejected_by_cryptokit(self):
        self.archive.write_bytes(b"different message")
        self.manifest()
        with self.assertRaisesRegex(RuntimeError, "public key"):
            self.create()
        self.assertFalse((self.root / "appcast.xml").exists())
    def test_ci_secret_uses_stdin_only(self):
        fixture_seed = base64.b64encode(bytes(32)).decode()
        with patch.dict(os.environ, {"SPARKLE_PRIVATE_KEY": fixture_seed}):
            self.create()
        command, options = self.calls[0]
        self.assertIn("--ed-key-file", command)
        self.assertNotIn(fixture_seed, command)
        self.assertEqual(options["input"], fixture_seed)
        self.assertNotIn("SPARKLE_PRIVATE_KEY", options["env"])
        self.assertNotIn("SPARKLE_PRIVATE_KEY", self.calls[1][1]["env"])
    def check_key(self):
        with patch.object(appcast, "ROOT", self.root), patch.object(appcast.subprocess, "run", self.run_command):
            appcast.check_key(Path("test-signer"))
    def test_probe_verifies_key_without_creating_feed(self):
        self.check_key()
        probe = Path(self.calls[0][0][-1])
        self.assertFalse(probe.exists())
        self.assertFalse((self.root / "appcast.xml").exists())
    def test_probe_rejects_invalid_secret_before_signing(self):
        with patch.dict(os.environ, {"SPARKLE_PRIVATE_KEY": "not-a-base64-key"}):
            with self.assertRaises(ValueError):
                self.check_key()
        self.assertFalse(self.calls)
    def test_probe_rejects_unrelated_key(self):
        with (self.root / "platform/macos/Info.plist.in").open("wb") as file:
            plistlib.dump({"SUPublicEDKey": base64.b64encode(bytes(32)).decode()}, file)
        with self.assertRaisesRegex(RuntimeError, "public key"):
            self.check_key()
        self.assertFalse(Path(self.calls[0][0][-1]).exists())
    def test_unsafe_manifest_path_rejected(self):
        with (self.root / "SHA256SUMS").open("a") as file:
            file.write("a" * 64 + "  ../outside.zip\n")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            self.create()
        self.assertFalse(self.calls)

if __name__ == "__main__":
    unittest.main()
