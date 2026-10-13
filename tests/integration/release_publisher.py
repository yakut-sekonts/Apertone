"""Verify publication ordering and refusal of incomplete or immutable assets. No network."""
import hashlib
import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

publisher = Path(__file__).resolve().parents[2] / "scripts/publish-release.py"

class ReleasePublisherTests(unittest.TestCase):
    def run_case(self, mode):
        with tempfile.TemporaryDirectory(prefix="luma-publisher-") as temporary:
            root = Path(temporary)
            assets = root / "dist/release"
            assets.mkdir(parents=True)
            for suffix in ("zip", "dmg"):
                (assets / f"Luma-0.3.0-macos-arm64.{suffix}").write_bytes(b"synthetic archive")
            info = {"version": "0.3.0", "source_commit": "a" * 40}
            if mode.startswith("signed-"):
                info["dependencies"] = {"Sparkle": "2.10.0"}
                if mode != "signed-missing":
                    signature = base64.b64encode(bytes(64)).decode()
                    feed_version = "0.2.0" if mode == "signed-wrong-version" else "0.3.0"
                    (assets / "appcast.xml").write_text(f'<rss xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle"><channel><item><enclosure url="https://github.com/yakut-sekonts/Luma/releases/download/v0.3.0/Luma-0.3.0-macos-arm64.zip" length="17" sparkle:version="{feed_version}" sparkle:edSignature="{signature}" /></item></channel></rss>')
            (assets / "BUILD_INFO.json").write_text(json.dumps(info))
            entries = []
            for path in sorted(assets.iterdir()):
                entries.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}")
            (assets / "SHA256SUMS").write_text("\n".join(entries) + "\n")
            fake = root / "gh"
            fake.write_text("#!/usr/bin/env python3\n" + '''
import hashlib, json, os, pathlib, sys
args = sys.argv[1:]
with pathlib.Path("calls.jsonl").open("a") as file: file.write(json.dumps(args) + "\\n")
mode = os.environ["MOCK_MODE"]
if args[:2] == ["release", "view"]:
    if mode == "published": print(json.dumps({"isDraft": False})); sys.exit(0)
    if pathlib.Path("created").exists() or mode == "resume":
        print(json.dumps({"isDraft": True, "databaseId": 123, "tagName": "v0.3.0"})); sys.exit(0)
    sys.exit(1)
if args[:2] == ["release", "create"]:
    pathlib.Path("created").touch()
if args[0] == "api":
    # The production bug: a draft is unavailable through releases/tags/<tag>.
    if args[1] != "repos/yakut-sekonts/Luma/releases/123": sys.exit(1)
    assets = []
    for path in pathlib.Path("dist/release").iterdir():
        assets.append({"name":path.name,"state":"uploaded","size":path.stat().st_size,
            "digest":"sha256:"+hashlib.sha256(path.read_bytes()).hexdigest()})
    if mode == "bad-digest": assets[0]["digest"] = "sha256:wrong"
    if mode == "incomplete": assets.pop()
    print(json.dumps({"html_url":"https://github.com/yakut-sekonts/Luma/releases/tag/v0.3.0","assets":assets}))
''')
            fake.chmod(0o755)
            env = {**os.environ, "PATH":str(root)+os.pathsep+str(Path(sys.executable).parent)+os.pathsep+os.environ["PATH"], "GITHUB_REPOSITORY":"yakut-sekonts/Luma", "GITHUB_REF_NAME":"v0.3.0", "GITHUB_SHA":"a"*40, "MOCK_MODE":mode}
            if mode == "wrong-commit": env["GITHUB_SHA"] = "b"*40
            if mode == "wrong-repo": env["GITHUB_REPOSITORY"] = "other/repo"
            result = subprocess.run([sys.executable,str(publisher)],cwd=root,env=env,capture_output=True,text=True,timeout=20)
            log = root / "calls.jsonl"
            calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
            return result, calls

    def test_complete_draft_publishes_last(self):
        result,calls = self.run_case("valid")
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual([call[:2] for call in calls],[['release','view'],['release','create'],['release','upload'],['release','view'],['api','repos/yakut-sekonts/Luma/releases/123'],['release','edit']])
        self.assertIn("--draft",calls[1])
        self.assertEqual(calls[1][calls[1].index("--title") + 1], "Luma 0.3.0")
        self.assertIn("--draft=false",calls[-1])

    def test_existing_draft_resumes_without_creating_another_release(self):
        result, calls = self.run_case("resume")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any(call[:2] == ["release", "create"] for call in calls))
        self.assertEqual(calls[-1][:2], ["release", "edit"])

    def test_signed_feed_is_uploaded_before_publication(self):
        result, calls = self.run_case("signed-valid")
        self.assertEqual(result.returncode, 0, result.stderr)
        upload = next(call for call in calls if call[:2] == ["release", "upload"])
        self.assertIn("dist/release/appcast.xml", upload)

    def test_invalid_assets_never_publish(self):
        for mode in ("bad-digest","incomplete","wrong-commit","wrong-repo","published","signed-missing","signed-wrong-version"):
            with self.subTest(mode=mode):
                result,calls=self.run_case(mode)
                self.assertNotEqual(result.returncode,0)
                self.assertFalse(any(call[:2]==["release","edit"] for call in calls))
                if mode=="published": self.assertFalse(any(call[:2]==["release","upload"] for call in calls))

if __name__ == "__main__":
    unittest.main()
