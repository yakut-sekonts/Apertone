"""Publish verified assets with the repository-scoped Actions token through gh.

Never retrieves a workstation credential or emits/saves a token.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from model_assets import describe_models, verify_zip
from release_identity import verify_zip_identity

repository = os.environ.get("GITHUB_REPOSITORY")
if repository != "yakut-sekonts/Apertone":
    raise SystemExit("This publisher is scoped to yakut-sekonts/Apertone.")
parser = argparse.ArgumentParser()
parser.add_argument("--tag", default=os.environ.get("GITHUB_REF_NAME", ""))
parser.add_argument("--source-commit", default=os.environ.get("GITHUB_SHA", ""))
parser.add_argument("--windows-folder", type=Path)
parser.add_argument("--create-tag", action="store_true")
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
major = int(version.split(".")[0])
if major >= 3 and arguments.windows_folder is None:
    raise SystemExit("Apertone 3.0 and later require qualified builds for both platforms.")
if major >= 1:
    if not info.get("dependencies", {}).get("Sparkle"):
        raise SystemExit("Apertone 1.0 and later require the signed Sparkle update channel.")
    if info.get("distribution") != "github-open-source":
        raise SystemExit("Apertone 1.0 and later require the explicit GitHub open-source distribution policy.")
    if info.get("signing") == "ad-hoc":
        if info.get("notarized") is not False or info.get("notarization"):
            raise SystemExit("Ad-hoc artifacts must explicitly declare that Apple notarization is absent.")
    elif info.get("signing") == "developer-id":
        receipts = info.get("notarization", {})
        if (info.get("notarized") is not True or
                any(receipts.get(kind, {}).get("status") != "Accepted" or
                    not re.fullmatch(r"[0-9a-fA-F-]{36}", receipts.get(kind, {}).get("submission_id", ""))
                    for kind in ("app", "dmg"))):
            raise SystemExit("Developer ID artifacts require accepted app/DMG notarization.")
    else:
        raise SystemExit("Unsupported release signing mode.")
paths = [folder / f"Apertone-{version}-macos-arm64.zip", folder / f"Apertone-{version}-macos-arm64.dmg", folder / "BUILD_INFO.json", folder / "SHA256SUMS"]
if info.get("dependencies", {}).get("Sparkle"):
    ns = "http://www.andymatuschak.org/xml-namespaces/sparkle"
    items = ET.parse(folder / "appcast.xml").findall("./channel/item")
    if len(items) != 1:
        raise SystemExit("Update feed must contain exactly this release.")
    enclosure = items[0].find("enclosure")
    if enclosure is None or enclosure.get(f"{{{ns}}}version") != version or enclosure.get("url") != f"https://github.com/{repository}/releases/download/{tag}/Apertone-{version}-macos-arm64.zip" or enclosure.get("length") != str(paths[0].stat().st_size):
        raise SystemExit("Update feed does not match the verified release ZIP.")
    if len(base64.b64decode(enclosure.get(f"{{{ns}}}edSignature", ""), validate=True)) != 64:
        raise SystemExit("Update feed requires an EdDSA signature.")
    paths.insert(-1, folder / "appcast.xml")
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
if major >= 1 or "bundled_models" in info:
    model_digest = info.get("models_manifest_sha256", "")
    if info.get("bundled_models") != describe_models() or not re.fullmatch(r"[0-9a-f]{64}", model_digest):
        raise SystemExit("Release requires both pinned offline models and their manifest checksum.")
    with zipfile.ZipFile(paths[0]) as archive:
        verify_zip(archive, model_digest)
        verify_zip_identity(archive, info)

if arguments.windows_folder is not None:
    windows_zip = arguments.windows_folder / f"Apertone-{version}-windows-x64.zip"
    windows_sums = arguments.windows_folder / "SHA256SUMS-windows.txt"
    with windows_zip.open("rb") as file:
        digest = hashlib.file_digest(file, "sha256").hexdigest()
    if windows_sums.read_text().splitlines() != [f"{digest}  {windows_zip.name}"]:
        raise SystemExit("Windows archive does not match its checksum manifest.")
    prefix = "Apertone-windows-x64/"
    with zipfile.ZipFile(windows_zip) as archive:
        members = archive.namelist()
        for name in ("Apertone.exe", "ApertoneUpdate.exe", "UPDATE-FILES.txt", "BUILD-ORIGIN.txt"):
            if members.count(prefix + name) != 1:
                raise SystemExit(f"Windows archive must contain one {name}.")
        build_origin = archive.read(prefix + "BUILD-ORIGIN.txt").decode("utf-8-sig").splitlines()
    expected_origin = [
        f"Repository: https://github.com/{repository}",
        f"Commit: {arguments.source_commit}",
        "Platform: Windows x64",
        f"Version: {version}",
    ]
    if len(build_origin) != 5 or any(build_origin.count(line) != 1 for line in expected_origin) or not any(
        re.fullmatch(r"Tree: [0-9a-f]{40}", line) for line in build_origin
    ):
        raise SystemExit("Windows archive version/source commit does not match the release tag.")
    paths.extend([windows_zip, windows_sums])
    checksums[windows_zip.name] = digest
    checksums[windows_sums.name] = hashlib.sha256(windows_sums.read_bytes()).hexdigest()

gh_executable = shutil.which("gh")
if gh_executable is None:
    raise SystemExit("GitHub CLI is unavailable.")

def gh(*args, check=True):
    result = subprocess.run([gh_executable, *args, "--repo", repository], capture_output=True, text=True)
    if check and result.returncode:
        raise SystemExit(f"GitHub CLI command failed ({result.returncode}); release remains unpublished.")
    return result

def github_api(resource, *args):
    return subprocess.run(
        [gh_executable, "api", f"repos/{repository}/{resource}", *args],
        capture_output=True,
        text=True,
    )

existing = gh("release", "view", tag, "--json", "isDraft", check=False)
if existing.returncode == 0 and not json.loads(existing.stdout)["isDraft"]:
    raise SystemExit("Release is already published; immutable assets will not be replaced.")
if arguments.create_tag:
    reference = f"git/ref/tags/{tag}"
    result = github_api(reference)
    if result.returncode:
        github_api("git/refs", "--method", "POST", "-f", f"ref=refs/tags/{tag}",
                   "-f", f"sha={arguments.source_commit}")
        result = github_api(reference)
    if result.returncode:
        raise SystemExit("Could not create or verify the release tag.")
    value = json.loads(result.stdout).get("object", {})
    if value.get("type") != "commit" or value.get("sha") != arguments.source_commit:
        raise SystemExit("The existing release tag refers to a different object.")
if existing.returncode:
    gh("release", "create", tag, "--verify-tag", "--draft", "--title", f"Apertone {version}", "--notes-file", f"docs/releases/{tag}.md")
gh("release", "upload", tag, *map(str, paths), "--clobber")
# The tag endpoint can return 404 for drafts. gh resolves drafts through the
# release list; use the resolved numeric ID for the asset REST request.
draft = json.loads(gh("release", "view", tag, "--json", "databaseId,isDraft,tagName").stdout)
if not draft["isDraft"] or draft["tagName"] != tag or type(draft["databaseId"]) is not int or draft["databaseId"] <= 0:
    raise SystemExit("Release is not the expected draft; refusing publication.")
result = github_api(f"releases/{draft['databaseId']}")
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
