#!/usr/bin/env python3
"""Deterministic Lensfun v1 XML → typed C++ data (data remains CC BY-SA 3.0)."""
import hashlib
import json
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

# Barrel markings and nominal focal lengths for measured manual primes.
# Keep coatings and model suffixes distinct when matching EXIF.
MANUAL_PRIMES = {
    ("KMZ", "MC MTO 11CA 10/1000"): ("MC MTO 11CA", "МС МТО 11СА", 1000),
    ("KMZ", "MC Helios-44M-4 58mm 1:2"): ("MC Helios-44M-4", "МС Гелиос-44М-4", 58),
    ("KMZ", "Helios-44 58mm 1:2"): ("Helios-44", "Гелиос-44", 58),
    ("KMZ", "Helios-40 85mm f/1.5"): ("Helios-40", "Гелиос-40", 85),
    ("MTO", "MTO-500 500mm f/8 mirror lens"): ("MTO-500", "МТО-500", 500),
    ("KMZ", "MIR-1B 37mm f/2.8"): ("Mir-1V", "Мир-1В", 37),
    ("KMZ", "Industar-50-2 3.5/50"): ("Industar-50-2", "Индустар-50-2", 50),
    ("KMZ", "Jupiter-37AM MC 3.5/135"): ("Jupiter-37AM MC", "Юпитер-37АМ МС", 135),
    ("Arsenal", "MC Volna-3 80mm f/2.8"): ("MC Volna-3", "МС Волна-3", 80),
    ("LZOS", "Industar-61 L/Z MC 50mm f/2.8"): ("Industar-61 L/Z MC", "Индустар-61 Л/З МС", 50),
    ("Zenit", "Zenitar MC 16mm f/2.8"): ("MC Zenitar 2.8/16", "МС Зенитар 2,8/16", 16),
}


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
    lines = ["// Lensfun calibration data, CC BY-SA 3.0; generated, do not edit.",
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

    supplement = Path(__file__).resolve().parents[1] / "resources/lens/industar-61.xml"
    for file in [*sorted(source.glob("*.xml")), supplement]:
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
            name = lens.findtext("model")
            maker = lens.findtext("maker")
            projection = lens.findtext("type", "rectilinear")
            # Apply only the measured radial distortion and TCA in the Zenitar's native
            # fisheye projection. The mapping does not convert to a rectilinear image.
            if projection != "rectilinear" and not (
                projection == "fisheye" and (maker, name) == ("Zenit", "Zenitar MC 16mm f/2.8")
            ):
                continue
            index = len(profiles)
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
            russian_name = lens.findtext("model[@lang='ru']", "") if file.name == "slr-ussr.xml" else ""
            fixed_focal = 0
            if prime := MANUAL_PRIMES.get((maker, name)):
                latin, russian_name, fixed_focal = prime
                models.extend([latin, russian_name])
            aliases = strings(f"lensAliases{index}", sorted(set(
                models + [a + " " + b for a in makers for b in models] +
                [b[len(a):].strip() for a in makers for b in models
                 if b.lower().startswith(a.lower() + " ")])))
            compatible = strings(f"lensMounts{index}", lens_mounts)
            data = [calibrations(f"lens{tag}{index}", lens, tag)
                    for tag in ["distortion", "tca", "vignetting"]]
            profiles.append("{" + ",".join([quoted(profile_id), quoted(name), aliases,
                                            compatible, crop, aspect, cx, cy, *data,
                                            number(fixed_focal), quoted(russian_name)]) + "}")
    for kind, name, entries in [("LensProfile", "profiles", profiles),
                                ("LensCamera", "cameras", cameras),
                                ("LensMount", "mounts", mounts)]:
        lines.append(f"static constexpr std::array<{kind}, {len(entries)}> {name}" +
                     "{{" + ",\n".join(entries) + "}};")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    build(Path(sys.argv[1]), Path(sys.argv[2]))
