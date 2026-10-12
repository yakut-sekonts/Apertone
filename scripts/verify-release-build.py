"""Validate a previously built tag before a publication-only recovery job."""
import json
import os
from pathlib import Path
import re
import subprocess

repository = os.environ["GITHUB_REPOSITORY"]
tag = os.environ["LUMA_RELEASE_TAG"]
run_id = os.environ["LUMA_BUILD_RUN_ID"]
if repository != "yakut-sekonts/Luma" or not re.fullmatch(r"v\d+\.\d+\.\d+", tag) or not re.fullmatch(r"[1-9]\d*", run_id):
    raise SystemExit("Invalid release repository, tag or build run.")

def api(endpoint):
    result = subprocess.run(["gh", "api", f"repos/{repository}/{endpoint}"], capture_output=True, text=True)
    if result.returncode:
        raise SystemExit("Cannot read release build metadata.")
    return json.loads(result.stdout)

source = subprocess.check_output(["git", "rev-parse", f"refs/tags/{tag}^{{commit}}"], text=True).strip()
subprocess.run(["git", "merge-base", "--is-ancestor", source, "origin/main"], check=True)
run = api(f"actions/runs/{run_id}")
if (run["status"] != "completed" or run["event"] != "push" or run["head_branch"] != tag
        or run["head_sha"] != source or run["path"] != ".github/workflows/release.yml"):
    raise SystemExit("Build run does not match the original release tag/workflow.")
jobs = api(f"actions/runs/{run_id}/jobs?per_page=100")["jobs"]
builders = [job for job in jobs if job["name"] == "build-package"]
if len(builders) != 1 or builders[0]["status"] != "completed" or builders[0]["conclusion"] != "success":
    raise SystemExit("Original release build, tests and bundle verification must have succeeded.")
artifacts = api(f"actions/runs/{run_id}/artifacts")["artifacts"]
packages = [artifact for artifact in artifacts if artifact["name"] == "macos-release"]
if len(packages) != 1 or packages[0]["expired"] or packages[0]["workflow_run"]["head_sha"] != source:
    raise SystemExit("Original verified release artifact is missing or expired.")
if not re.fullmatch(r"sha256:[0-9a-f]{64}", packages[0].get("digest", "")):
    raise SystemExit("Original release artifact has no SHA-256 digest.")
with Path(os.environ["GITHUB_OUTPUT"]).open("a") as file:
    file.write(f"source_sha={source}\ntag={tag}\nrun_id={run_id}\n")
print(f"Verified {tag}: source {source}, successful build-package job and original artifact.")
