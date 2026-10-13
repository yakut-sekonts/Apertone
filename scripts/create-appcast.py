"""Sign a verified release ZIP and add its Sparkle feed to the manifest.

Locally uses the dedicated macOS Keychain account. Actions passes its secret on
stdin to Sparkle's signing tool; no private key is written or printed.
"""
import argparse
import base64
from datetime import datetime, timezone
import email.utils
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NAMESPACE = "http://www.andymatuschak.org/xml-namespaces/sparkle"
ET.register_namespace("sparkle", NAMESPACE)

def create(folder, signer):
    info = json.loads((folder / "BUILD_INFO.json").read_text())
    version = info["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("A stable semantic version is required")
    if not re.fullmatch(r"[0-9a-f]{40}", info["source_commit"]):
        raise ValueError("A full source commit is required")
    archive = folder / f"Luma-{version}-macos-arm64.zip"
    expected = {archive.name, f"Luma-{version}-macos-arm64.dmg", "BUILD_INFO.json"}
    manifest = {}
    for line in (folder / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        if name in manifest or name not in expected or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("Unexpected or duplicate checksum entry")
        manifest[name] = digest
    if set(manifest) != expected:
        raise ValueError("Incomplete release manifest")
    for name, digest in manifest.items():
        with (folder / name).open("rb") as file:
            if hashlib.file_digest(file, "sha256").hexdigest() != digest:
                raise ValueError("Release checksum mismatch")
    with (ROOT / "platform/macos/Info.plist.in").open("rb") as file:
        public_key = plistlib.load(file)["SUPublicEDKey"]
    secret = os.environ.get("SPARKLE_PRIVATE_KEY")
    command = [str(signer), "-p"]
    if secret:
        if len(base64.b64decode(secret.strip(), validate=True)) not in (32, 96):
            raise ValueError("Unsupported Sparkle signing key")
        command += ["--ed-key-file", "-"]
    else:
        command += ["--account", "yakut-sekonts.Luma"]
    command.append(str(archive.resolve()))
    safe_env = {key: value for key, value in os.environ.items() if key != "SPARKLE_PRIVATE_KEY"}
    result = subprocess.run(command, input=secret, capture_output=True, text=True,
                            env=safe_env, timeout=120)
    if result.returncode:
        raise RuntimeError("Sparkle signing failed; no feed was published")
    signature = result.stdout.strip()
    if len(base64.b64decode(signature, validate=True)) != 64:
        raise ValueError("Invalid EdDSA signature")
    verification = subprocess.run(["swift", str(ROOT / "scripts/verify-update-signature.swift"),
        str(archive.resolve()), public_key, signature], capture_output=True, env=safe_env, timeout=120)
    if verification.returncode:
        raise RuntimeError("Signature does not match Luma's embedded public key")
    rss = ET.Element("rss", {"version": "2.0"})
    channel = ET.SubElement(rss, "channel")
    ET.SubElement(channel, "title").text = "Luma updates"
    item = ET.SubElement(channel, "item")
    ET.SubElement(item, "title").text = f"Luma {version}"
    ET.SubElement(item, "pubDate").text = email.utils.format_datetime(datetime.now(timezone.utc))
    ET.SubElement(item, f"{{{NAMESPACE}}}minimumSystemVersion").text = "14.0"
    ET.SubElement(item, f"{{{NAMESPACE}}}releaseNotesLink").text = (
        f"https://github.com/yakut-sekonts/Luma/releases/tag/v{version}")
    ET.SubElement(item, "enclosure", {
        "url": f"https://github.com/yakut-sekonts/Luma/releases/download/v{version}/{archive.name}",
        "length": str(archive.stat().st_size), "type": "application/octet-stream",
        f"{{{NAMESPACE}}}version": version, f"{{{NAMESPACE}}}shortVersionString": version,
        f"{{{NAMESPACE}}}edSignature": signature,
    })
    ET.indent(rss)
    feed = folder / "appcast.xml"
    temporary = folder / "appcast.xml.partial"
    ET.ElementTree(rss).write(temporary, encoding="utf-8", xml_declaration=True)
    temporary.replace(feed)
    manifest[feed.name] = hashlib.sha256(feed.read_bytes()).hexdigest()
    (folder / "SHA256SUMS").write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(manifest.items())))
    print(f"Signed Sparkle feed prepared for Luma {version}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--folder", type=Path, default=ROOT / "dist/release")
    parser.add_argument("--signer", type=Path, required=True)
    args = parser.parse_args()
    create(args.folder, args.signer)
