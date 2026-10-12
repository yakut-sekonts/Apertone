"""Publish verified assets with the repository-scoped Actions token through gh.

Never retrieves a workstation credential or emits/saves a token.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

repository = os.environ.get("GITHUB_REPOSITORY")
if repository != "yakut-sekonts/Luma":
    raise SystemExit("This publisher is scoped to yakut-sekonts/Luma.")
parser = argparse.ArgumentParser()
parser.add_argument("--tag", default=os.environ.get("GITHUB_REF_NAME", ""))
parser.add_argument("--source-commit", default=os.environ.get("GITHUB_SHA", ""))
arguments = parser.parse_args()
tag = arguments.tag
if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
    raise SystemExit("Release requires a semantic-version tag.")
version = tag[1:]
folder = Path("dist/release")
info = json.loads((folder / "BUILD_INFO.json").read_text())
if not re.fullmatch(r"[0-9a-f]{40}", arguments.source_commit):
    raise SystemExit("Release requires a full source commit SHA.")
if info["version"] != version or info["source_commit"] != arguments.source_commit:
    raise SystemExit("Asset version/source commit does not match the release tag.")
paths = [folder / f"Luma-{version}-macos-arm64.zip", folder / f"Luma-{version}-macos-arm64.dmg", folder / "BUILD_INFO.json", folder / "SHA256SUMS"]
manifest = {}
for line in (folder / "SHA256SUMS").read_text().splitlines():
    digest, name = line.split("  ", 1)
    if name in manifest:
        raise SystemExit("Duplicate checksum entry.")
    manifest[name] = digest
checksums = {}
for path in paths:
    with path.open("rb") as file:
        checksums[path.name] = hashlib.file_digest(file, "sha256").hexdigest()
if set(manifest) != {path.name for path in paths[:-1]} or any(checksums[name] != digest for name, digest in manifest.items()):
    raise SystemExit("Release checksum manifest is invalid.")

def gh(*args, check=True):
    result = subprocess.run(["gh", *args, "--repo", repository], capture_output=True, text=True)
    if check and result.returncode:
        raise SystemExit(f"GitHub CLI command failed ({result.returncode}); release remains unpublished.")
    return result

existing = gh("release", "view", tag, "--json", "isDraft", check=False)
if existing.returncode == 0 and not json.loads(existing.stdout)["isDraft"]:
    raise SystemExit("Release is already published; immutable assets will not be replaced.")
if existing.returncode:
    gh("release", "create", tag, "--verify-tag", "--draft", "--title", f"Luma {version} — первый релиз для macOS", "--notes-file", f"docs/releases/{tag}.md")
gh("release", "upload", tag, *map(str, paths), "--clobber")
# The tag endpoint can return 404 for drafts. gh resolves drafts through the
# release list; use the resolved numeric ID for the asset REST request.
draft = json.loads(gh("release", "view", tag, "--json", "databaseId,isDraft,tagName").stdout)
if not draft["isDraft"] or draft["tagName"] != tag or type(draft["databaseId"]) is not int or draft["databaseId"] <= 0:
    raise SystemExit("Release is not the expected draft; refusing publication.")
result = subprocess.run(["gh", "api", f"repos/{repository}/releases/{draft['databaseId']}"], capture_output=True, text=True)
if result.returncode:
    raise SystemExit("Cannot verify draft release assets by ID; release remains unpublished.")
release = json.loads(result.stdout)
assets = {asset["name"]: asset for asset in release["assets"]}
if set(assets) != set(checksums):
    raise SystemExit("Unexpected release asset set; release remains a draft.")
for path in paths:
    asset = assets[path.name]
    if asset["state"] != "uploaded" or asset["size"] != path.stat().st_size or asset.get("digest") != "sha256:" + checksums[path.name]:
        raise SystemExit("Uploaded asset verification failed; release remains a draft.")
gh("release", "edit", tag, "--draft=false", "--latest")
print(release["html_url"])
