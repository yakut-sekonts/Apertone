#!/usr/bin/env python3
"""Deterministic Lensfun v1 XML → typed C++ data (data remains CC BY-SA 3.0)."""
import hashlib
import json
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def number(value):
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("Non-finite calibration")
    return repr(result)


def quoted(value):
    return json.dumps(value, ensure_ascii=False)


def texts(node, name):
    return sorted(set(item.text.strip() for item in node.findall(name) if item.text))


def build(source, destination):
    lines = ["// Lensfun v0.3.4 database, CC BY-SA 3.0; generated, do not edit.",
             "// Lensfun contributors / Tom Niemann; see THIRD_PARTY_LICENSES.md."]
    profiles, cameras, mounts = [], [], []
    identities = set()

    def strings(name, values):
        lines.append(f"static constexpr std::array<std::string_view, {len(values)}> {name}{{" +
                     ",".join(map(quoted, values)) + "};")
        return name

    def calibrations(name, node, tag):
        fields = {"poly3": [("k1", 0)], "poly5": [("k1", 0), ("k2", 0)],
                  "ptlens": [("a", 0), ("b", 0), ("c", 0)],
                  "linear": [("kr", 1), ("kb", 1)],
                  "pa": [("k1", 0), ("k2", 0), ("k3", 0)]}
        if tag == "tca":
            fields["poly3"] = [("br", 0), ("cr", 0), ("vr", 1),
                               ("bb", 0), ("cb", 0), ("vb", 1)]
        entries = []
        for item in node.findall(f"calibration/{tag}"):
            model = item.get("model")
            if model not in fields:
                continue
            terms = [number(item.get(key, default)) for key, default in fields[model]]
            terms += ["0.0"] * (6 - len(terms))
            entries.append("{" + ",".join([quoted(model), number(item.get("focal")),
                                            number(item.get("aperture", 0)),
                                            number(item.get("distance", 0)),
                                            "{" + ",".join(terms) + "}"]) + "}")
        entries.sort()
        lines.append(f"static constexpr std::array<LensCalibration, {len(entries)}> {name}" +
                     "{{" + ",".join(entries) + "}};")
        return name

    for file in sorted(source.glob("*.xml")):
        root = ET.parse(file).getroot()
        if root.get("version") != "1":
            raise ValueError("Unsupported lens database version")
        for mount in root.findall("mount"):
            index = len(mounts)
            compat = strings(f"mountCompat{index}", texts(mount, "compat"))
            mounts.append("{" + quoted(mount.findtext("name")) + "," + compat + "}")
        for camera in root.findall("camera"):
            index = len(cameras)
            makers, models = texts(camera, "maker"), texts(camera, "model")
            aliases = strings(f"cameraAliases{index}", sorted(set(
                models + [a + " " + b for a in makers for b in models])))
            # A maker alone is deliberately excluded from camera matching below.
            name = camera.findtext("maker") + " " + camera.findtext("model")
            cameras.append("{" + ",".join([quoted(name), aliases,
                                            quoted(camera.findtext("mount")),
                                            number(camera.findtext("cropfactor"))]) + "}")
        for lens in root.findall("lens"):
            if lens.findtext("type", "rectilinear") != "rectilinear":
                continue
            index = len(profiles)
            name = lens.findtext("model")
            crop = number(lens.findtext("cropfactor"))
            aspect = lens.findtext("aspect-ratio", "3:2")
            aspect = number(float(aspect.split(":")[0]) / float(aspect.split(":")[1])
                            if ":" in aspect else aspect)
            center = lens.find("center")
            cx = number(center.get("x", 0) if center is not None else 0)
            cy = number(center.get("y", 0) if center is not None else 0)
            lens_mounts = texts(lens, "mount")
            identity = json.dumps([lens.findtext("maker"), name, lens_mounts, crop, aspect, cx, cy])
            profile_id = hashlib.sha256(identity.encode()).hexdigest()[:32]
            if profile_id in identities:
                raise ValueError("Duplicate lens identity")
            identities.add(profile_id)
            models, makers = texts(lens, "model"), texts(lens, "maker")
            aliases = strings(f"lensAliases{index}", sorted(set(
                models + [a + " " + b for a in makers for b in models] +
                [b[len(a):].strip() for a in makers for b in models
                 if b.lower().startswith(a.lower() + " ")])))
            compatible = strings(f"lensMounts{index}", lens_mounts)
            data = [calibrations(f"lens{tag}{index}", lens, tag)
                    for tag in ["distortion", "tca", "vignetting"]]
            profiles.append("{" + ",".join([quoted(profile_id), quoted(name), aliases,
                                            compatible, crop, aspect, cx, cy, *data]) + "}")
    for kind, name, entries in [("LensProfile", "profiles", profiles),
                                ("LensCamera", "cameras", cameras),
                                ("LensMount", "mounts", mounts)]:
        lines.append(f"static constexpr std::array<{kind}, {len(entries)}> {name}" +
                     "{{" + ",\n".join(entries) + "}};")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    build(Path(sys.argv[1]), Path(sys.argv[2]))
