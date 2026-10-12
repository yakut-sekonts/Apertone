"""A publication retry must use the successful build of the original tag."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

verifier = Path(__file__).resolve().parents[2] / "scripts/verify-release-build.py"


class ReleaseBuildTests(unittest.TestCase):
    def run_case(self, mode):
        with tempfile.TemporaryDirectory(prefix="luma-release-build-") as temporary:
            root = Path(temporary)
            run = {"status": "completed", "event": "push", "head_branch": "v0.3.0",
                   "head_sha": "a" * 40, "path": ".github/workflows/release.yml"}
            jobs = {"jobs": [{"name": "build-package", "status": "completed", "conclusion": "success"}]}
            artifact = {"name": "macos-release", "expired": False, "digest": "sha256:" + "b" * 64,
                        "workflow_run": {"head_sha": "a" * 40}}
            artifacts = {"artifacts": [artifact]}
            if mode == "wrong-source": run["head_sha"] = "c" * 40
            if mode == "wrong-tag": run["head_branch"] = "v0.2.0"
            if mode == "wrong-event": run["event"] = "pull_request"
            if mode == "wrong-workflow": run["path"] = ".github/workflows/macos.yml"
            if mode == "failed-build": jobs["jobs"][0]["conclusion"] = "failure"
            if mode == "duplicate-build": jobs["jobs"].append(copy.deepcopy(jobs["jobs"][0]))
            if mode == "expired": artifact["expired"] = True
            if mode == "wrong-artifact-source": artifact["workflow_run"]["head_sha"] = "c" * 40
            if mode == "missing-digest": artifact["digest"] = None
            if mode == "missing-artifact": artifacts["artifacts"] = []
            (root / "fixtures.json").write_text(json.dumps({"run": run, "jobs": jobs, "artifacts": artifacts}))
            fake = root / "gh"
            fake.write_text('''#!/usr/bin/env python3
import json, pathlib, sys
endpoint = sys.argv[2]
fixtures = json.loads(pathlib.Path("fixtures.json").read_text())
key = "jobs" if endpoint.endswith("/jobs?per_page=100") else "artifacts" if endpoint.endswith("/artifacts") else "run"
print(json.dumps(fixtures[key]))
''')
            fake.chmod(0o755)
            fake = root / "git"
            fake.write_text('''#!/usr/bin/env python3
import os, sys
if sys.argv[1] == "rev-parse": print("a" * 40)
elif sys.argv[1] == "merge-base": sys.exit(1 if os.environ["MOCK_MODE"] == "outside-main" else 0)
else: sys.exit(2)
''')
            fake.chmod(0o755)
            output = root / "output"
            env = {**os.environ, "PATH": f"{root}:{Path(sys.executable).parent}:{os.environ['PATH']}",
                   "GITHUB_REPOSITORY": "yakut-sekonts/Luma", "LUMA_RELEASE_TAG": "v0.3.0",
                   "LUMA_BUILD_RUN_ID": "123", "GITHUB_OUTPUT": str(output), "MOCK_MODE": mode}
            result = subprocess.run([sys.executable, str(verifier)], cwd=root, env=env,
                                    capture_output=True, text=True, timeout=20)
            return result, output.read_text() if output.exists() else ""

    def test_original_verified_build_outputs_its_source(self):
        result, output = self.run_case("valid")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output, f"source_sha={'a' * 40}\ntag=v0.3.0\nrun_id=123\n")

    def test_unverified_or_unrelated_builds_never_reach_publication(self):
        for mode in ("wrong-source", "wrong-tag", "wrong-event", "wrong-workflow", "failed-build",
                     "duplicate-build", "expired", "wrong-artifact-source", "missing-digest",
                     "missing-artifact", "outside-main"):
            with self.subTest(mode=mode):
                result, output = self.run_case(mode)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(output, "")


if __name__ == "__main__":
    unittest.main()
