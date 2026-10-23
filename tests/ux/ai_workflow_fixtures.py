"""Pinned redistributable photographs for actual local AI workflow checks."""
import hashlib
from pathlib import Path
import shutil

CASE_PROFILE_IDS = {'UX-P07-MSK-001', 'UX-P07-MSK-003'}
ROOT = Path(__file__).resolve().parents[2]
PORTRAIT_SHA256 = 'b7224c8c68fc7aaf76657b0d039e9fac69c00117d58523e5d2a11fe7cd2fbe61'


def official_fixture(folder, *, owned_root, case_id):
    folder, owned_root = Path(folder), Path(owned_root)
    if (not owned_root.is_absolute() or owned_root.resolve() != owned_root
            or not owned_root.is_dir() or owned_root.is_symlink()):
        raise ValueError('AI workflow requires a canonical owned run directory')
    if case_id not in CASE_PROFILE_IDS or folder != owned_root / 'case-fixtures' / case_id:
        raise ValueError('AI workflow requires its exact owned case directory')
    if folder.resolve() != folder or folder.exists() or folder.is_symlink():
        raise ValueError('AI workflow requires a fresh directory without aliases')
    source = ROOT / 'tests/assets/filters/portrait.png'
    if source.is_symlink() or hashlib.sha256(source.read_bytes()).hexdigest() != PORTRAIT_SHA256:
        raise ValueError('Pinned public-domain portrait is missing or changed')
    folder.mkdir(parents=True)
    copied = folder / 'Portrait.png'
    shutil.copyfile(source, copied)
    if hashlib.sha256(copied.read_bytes()).hexdigest() != PORTRAIT_SHA256:
        raise ValueError('Owned portrait copy changed')
    return {str(copied): PORTRAIT_SHA256}, {
        'case_id': case_id, 'name': 'actual-local-ai-portrait-v1', 'directory': str(folder),
        'public_photo_count': 1, 'original_checksum_count': 1, 'large_24mp_generated': False,
        'source_license_manifest': 'tests/assets/filters/manifest.json', 'width': 512, 'height': 512,
    }
