"""Verify publication ordering and refusal of incomplete or immutable assets. No network."""
import hashlib
import base64
import json
import os
import plistlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

publisher = Path(__file__).resolve().parents[2] / "scripts/publish-release.py"
sys.path.insert(0, str(publisher.parent))
from model_assets import SOURCES, describe_models, digest, recipe_digest


class ReleasePublisherTests(unittest.TestCase):
    def run_case(self, mode, version="0.3.0", info_override=None, archive_mode=None,
                 windows_mode=None, create_tag=False):
        with tempfile.TemporaryDirectory(prefix="luma-publisher-") as temporary:
            root = Path(temporary)
            assets = root / "dist/release"
            assets.mkdir(parents=True)
            for suffix in ("zip", "dmg"):
                (assets / f"Apertone-{version}-macos-arm64.{suffix}").write_bytes(b"synthetic archive")
            info = {"version": version, "source_commit": "a" * 40}
            if int(version.split(".")[0]) >= 1:
                # Structural package fixtures only, not executable Core ML models.
                models = describe_models()
                files = {model["bundle"] + "/fixture.bin": b"synthetic compiled-model member"
                         for model in models}
                manifest = {"schema": 1, "recipe_sha256": recipe_digest(),
                            "source_manifest_sha256": digest(SOURCES), "models": models,
                            "files": {name: {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                                      for name, data in files.items()}}
                raw = json.dumps(manifest).encode()
                with zipfile.ZipFile(assets / f"Apertone-{version}-macos-arm64.zip", "w") as archive:
                    prefix = "Apertone.app/Contents/Resources/Models/"
                    archive.writestr(prefix + "manifest.json", raw)
                    for name, data in files.items():
                        archive.writestr(prefix + name, data)
                info.update(distribution="github-open-source", signing="ad-hoc", notarized=False,
                            bundled_models=models, models_manifest_sha256=hashlib.sha256(raw).hexdigest())
            if mode.startswith("signed-") or int(version.split(".")[0]) >= 1:
                info["dependencies"] = {"Sparkle": "2.10.0"}
            info.update(info_override or {})
            if int(version.split(".")[0]) >= 1:
                embedded = dict(info)
                if embedded.get("signing") == "developer-id":
                    embedded.pop("notarized", None)
                    embedded.pop("notarization", None)
                plist = {"CFBundleIdentifier": "app.luma.desktop", "CFBundleExecutable": "Apertone",
                         "CFBundleShortVersionString": version, "CFBundleVersion": version}
                if archive_mode == "stale-source": embedded["source_commit"] = "c" * 40
                if archive_mode == "stale-version": embedded["version"] = "0.9.0"
                if archive_mode == "different-signing": embedded["signing"] = "developer-id"
                if archive_mode == "different-models": embedded["models_manifest_sha256"] = "0" * 64
                if archive_mode == "wrong-plist-version": plist["CFBundleShortVersionString"] = "0.9.0"
                if archive_mode == "wrong-plist-build": plist["CFBundleVersion"] = "0.9.0"
                if archive_mode == "wrong-app-id": plist["CFBundleIdentifier"] = "app.other.desktop"
                if archive_mode == "wrong-executable": plist["CFBundleExecutable"] = "Other"
                with zipfile.ZipFile(assets / f"Apertone-{version}-macos-arm64.zip", "a") as archive:
                    if archive_mode != "missing-build-info":
                        archive.writestr("Apertone.app/Contents/Resources/BUILD_INFO.json", json.dumps(embedded))
                    archive.writestr("Apertone.app/Contents/Info.plist", plistlib.dumps(plist))
                    archive.writestr("Apertone.app/Contents/MacOS/Apertone", b"synthetic executable fixture")
            if info.get("dependencies", {}).get("Sparkle") and mode != "signed-missing":
                signature = base64.b64encode(bytes(64)).decode()
                feed_version = "0.2.0" if mode == "signed-wrong-version" else version
                (assets / "appcast.xml").write_text(f'<rss xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle"><channel><item><enclosure url="https://github.com/yakut-sekonts/Apertone/releases/download/v{version}/Apertone-{version}-macos-arm64.zip" length="{(assets / f'Apertone-{version}-macos-arm64.zip').stat().st_size}" sparkle:version="{feed_version}" sparkle:edSignature="{signature}" /></item></channel></rss>')
            (assets / "BUILD_INFO.json").write_text(json.dumps(info))
            entries = []
            for path in sorted(assets.iterdir()):
                entries.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}")
            (assets / "SHA256SUMS").write_text("\n".join(entries) + "\n")
            arguments = []
            if windows_mode is not None:
                windows = root / "dist/release-windows"
                windows.mkdir()
                package = windows / f"Apertone-{version}-windows-x64.zip"
                commit = ("b" if windows_mode == "stale-source" else "a") * 40
                win_version = "2.5.0" if windows_mode == "stale-version" else version
                origin = (
                    "Repository: https://github.com/yakut-sekonts/Apertone\n"
                    f"Commit: {commit}\nTree: {'c' * 40}\nPlatform: Windows x64\nVersion: {win_version}\n"
                )
                with zipfile.ZipFile(package, "w") as archive:
                    prefix = "Apertone-windows-x64/"
                    archive.writestr(prefix + "BUILD-ORIGIN.txt", origin)
                    archive.writestr(prefix + "Apertone.exe", b"synthetic desktop")
                    if windows_mode != "missing-updater":
                        archive.writestr(prefix + "ApertoneUpdate.exe", b"synthetic updater")
                    archive.writestr(prefix + "UPDATE-FILES.txt", b"fixture inventory")
                checksum = "0" * 64 if windows_mode == "bad-checksum" else hashlib.sha256(package.read_bytes()).hexdigest()
                (windows / "SHA256SUMS-windows.txt").write_text(f"{checksum}  {package.name}\n")
                arguments = ["--windows-folder", "dist/release-windows"]
            if create_tag:
                arguments.append("--create-tag")
            fake = root / "gh"
            fake.write_text("#!/usr/bin/env python3\n" + '''
import hashlib, json, os, pathlib, sys
args = sys.argv[1:]
with pathlib.Path("calls.jsonl").open("a") as file: file.write(json.dumps(args) + "\\n")
mode = os.environ["MOCK_MODE"]
version = os.environ["MOCK_VERSION"]
if args[:2] == ["release", "view"]:
    if mode == "published": print(json.dumps({"isDraft": False})); sys.exit(0)
    if pathlib.Path("created").exists() or mode == "resume":
        print(json.dumps({"isDraft": True, "databaseId": 123, "tagName": "v" + version})); sys.exit(0)
    sys.exit(1)
if args[:2] == ["release", "create"]:
    pathlib.Path("created").touch()
if args[:2] == ["release", "upload"]:
    pathlib.Path("uploaded.json").write_text(json.dumps(args[3:args.index("--clobber")]))
if args[0] == "api":
    if args[1] == "repos/yakut-sekonts/Apertone/git/ref/tags/v" + version:
        if mode == "tag-conflict":
            print(json.dumps({"object": {"type": "commit", "sha": "b"*40}})); sys.exit(0)
        if not pathlib.Path("tagged").exists(): sys.exit(1)
        print(json.dumps({"object": {"type": "commit", "sha": "a"*40}})); sys.exit(0)
    if args[1] == "repos/yakut-sekonts/Apertone/git/refs":
        if args[2:] != ["--method", "POST", "-f", "ref=refs/tags/v" + version, "-f", "sha=" + "a"*40]:
            sys.exit(1)
        pathlib.Path("tagged").touch()
        sys.exit(0)
    # The production bug: a draft is unavailable through releases/tags/<tag>.
    if args[1] != "repos/yakut-sekonts/Apertone/releases/123": sys.exit(1)
    assets = []
    for path in map(pathlib.Path, json.loads(pathlib.Path("uploaded.json").read_text())):
        assets.append({"name":path.name,"state":"uploaded","size":path.stat().st_size,
            "digest":"sha256:"+hashlib.sha256(path.read_bytes()).hexdigest()})
    if mode == "bad-digest": assets[0]["digest"] = "sha256:wrong"
    if mode == "incomplete": assets.pop()
    print(json.dumps({"html_url":"https://github.com/yakut-sekonts/Apertone/releases/tag/v" + version,"assets":assets}))
''')
            fake.chmod(0o755)
            if os.name == "nt":
                fake.with_suffix(".cmd").write_text(f'@"{sys.executable}" "%~dp0gh" %*\n')
            env = {**os.environ, "PATH":str(root)+os.pathsep+str(Path(sys.executable).parent)+os.pathsep+os.environ["PATH"], "GITHUB_REPOSITORY":"yakut-sekonts/Apertone", "GITHUB_REF_NAME":"v"+version, "GITHUB_SHA":"a"*40, "MOCK_MODE":mode, "MOCK_VERSION":version}
            if mode == "wrong-commit": env["GITHUB_SHA"] = "b"*40
            if mode == "wrong-repo": env["GITHUB_REPOSITORY"] = "other/repo"
            result = subprocess.run([sys.executable, str(publisher), *arguments], cwd=root, env=env,
                                    capture_output=True, text=True, timeout=20)
            log = root / "calls.jsonl"
            calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
            return result, calls

    def test_complete_draft_publishes_last(self):
        result,calls = self.run_case("valid")
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual([call[:2] for call in calls],[['release','view'],['release','create'],['release','upload'],['release','view'],['api','repos/yakut-sekonts/Apertone/releases/123'],['release','edit']])
        self.assertIn("--draft",calls[1])
        self.assertEqual(calls[1][calls[1].index("--title") + 1], "Apertone 0.3.0")
        self.assertIn("--draft=false",calls[-1])

    def test_version_three_publishes_both_platforms_together(self):
        result, calls = self.run_case("valid", "3.0.0", windows_mode="valid")
        self.assertEqual(result.returncode, 0, result.stderr)
        upload = next(call for call in calls if call[:2] == ["release", "upload"])
        names = {Path(argument).name for argument in upload}
        self.assertIn("Apertone-3.0.0-windows-x64.zip", names)
        self.assertIn("Apertone-3.0.0-macos-arm64.zip", names)
        self.assertIn("SHA256SUMS-windows.txt", names)
        self.assertEqual(calls[-1][:2], ["release", "edit"])

    def test_version_three_refuses_unqualified_windows_archive(self):
        for windows_mode in (None, "stale-source", "stale-version", "bad-checksum", "missing-updater"):
            with self.subTest(windows_mode=windows_mode):
                result, calls = self.run_case("valid", "3.0.0", windows_mode=windows_mode)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(calls, [])

    def test_dispatch_creates_exact_tag_after_local_artifact_checks(self):
        result, calls = self.run_case("valid", "3.0.0", windows_mode="valid", create_tag=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        create_tag_call = next(i for i, call in enumerate(calls)
                               if call[:2] == ["api", "repos/yakut-sekonts/Apertone/git/refs"])
        draft_call = next(i for i, call in enumerate(calls) if call[:2] == ["release", "create"])
        self.assertLess(create_tag_call, draft_call)
        self.assertEqual(calls[-1][:2], ["release", "edit"])

    def test_dispatch_does_not_retarget_existing_tag(self):
        result, calls = self.run_case("tag-conflict", "3.0.0", windows_mode="valid", create_tag=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(calls), 2)

    def test_existing_draft_resumes_without_creating_another_release(self):
        result, calls = self.run_case("resume")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any(call[:2] == ["release", "create"] for call in calls))
        self.assertEqual(calls[-1][:2], ["release", "edit"])

    def test_signed_feed_is_uploaded_before_publication(self):
        result, calls = self.run_case("signed-valid")
        self.assertEqual(result.returncode, 0, result.stderr)
        upload = next(call for call in calls if call[:2] == ["release", "upload"])
        self.assertIn(Path("dist/release/appcast.xml"), list(map(Path, upload)))

    def test_open_source_1_0_publishes_verified_assets_and_signed_update_last(self):
        result, calls = self.run_case("valid", "1.0.0")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[-1][:2], ["release", "edit"])
        upload = next(call for call in calls if call[:2] == ["release", "upload"])
        uploaded = list(map(Path, upload))
        self.assertIn(Path("dist/release/appcast.xml"), uploaded)
        self.assertIn(Path("dist/release/Apertone-1.0.0-macos-arm64.dmg"), uploaded)

    def test_1_0_refuses_false_signing_claims_and_missing_distribution(self):
        overrides = [
            {"distribution": None}, {"signing": "unsigned"}, {"notarized": None},
            {"notarized": True}, {"notarization": {"app": {"status": "Accepted"}}},
            {"signing": "developer-id", "notarized": False},
            {"signing": "developer-id", "notarized": True},
            {"bundled_models": []}, {"models_manifest_sha256": "0" * 64},
            {"dependencies": {}},
        ]
        for override in overrides:
            with self.subTest(override=override):
                result, calls = self.run_case("valid", "1.0.0", override)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(calls, [], "Invalid local provenance contacted GitHub")

    def test_archive_identity_must_match_release_before_any_github_request(self):
        for mode in ("stale-source", "stale-version", "different-signing", "different-models",
                     "wrong-plist-version", "wrong-plist-build", "wrong-app-id",
                     "wrong-executable", "missing-build-info"):
            with self.subTest(mode=mode):
                result, calls = self.run_case("valid", "1.0.0", archive_mode=mode)
                self.assertNotEqual(result.returncode, 0, "Mismatched archive was published")
                self.assertEqual(calls, [], "Mismatched archive contacted GitHub")

    def test_optional_developer_id_requires_both_accepted_receipts(self):
        receipts = {kind: {"status": "Accepted", "submission_id": "12345678-1234-1234-1234-123456789abc"}
                    for kind in ("app", "dmg")}
        info = {"signing": "developer-id", "notarized": True, "notarization": receipts}
        result, calls = self.run_case("valid", "1.0.0", info)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[-1][:2], ["release", "edit"])
        receipts["dmg"]["status"] = "Rejected"
        result, calls = self.run_case("valid", "1.0.0", info)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_open_source_release_still_rejects_missing_or_incorrect_update_feed(self):
        for mode in ("signed-missing", "signed-wrong-version", "bad-digest", "incomplete"):
            with self.subTest(mode=mode):
                result, calls = self.run_case(mode, "1.0.0")
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(any(call[:2] == ["release", "edit"] for call in calls))

    def test_invalid_assets_never_publish(self):
        for mode in ("bad-digest","incomplete","wrong-commit","wrong-repo","published","signed-missing","signed-wrong-version"):
            with self.subTest(mode=mode):
                result,calls=self.run_case(mode)
                self.assertNotEqual(result.returncode,0)
                self.assertFalse(any(call[:2]==["release","edit"] for call in calls))
                if mode=="published": self.assertFalse(any(call[:2]==["release","upload"] for call in calls))

if __name__ == "__main__":
    unittest.main()
