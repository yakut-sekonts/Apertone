#!/usr/bin/env python3
"""Two-file native minimize/restore export candidate, without load loops."""
import hashlib
from pathlib import Path
import struct
import zlib

import run_ux_suite as runner

IDENTIFIER = 'CANDIDATE-APP016-MINIMIZED-EXPORT'


def two_photos(folder):
    folder = Path(folder)
    if not folder.is_absolute() or folder.resolve() != folder or folder.exists():
        raise ValueError('Minimized export requires a fresh canonical fixture directory')
    folder.mkdir(parents=True, exist_ok=False)

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))

    for index, color in enumerate(((40, 80, 120), (160, 120, 80))):
        data = (b'\x89PNG\r\n\x1a\n'
                + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 48, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress((b'\0' + bytes(color) * 64) * 48))
                + chunk(b'IEND', b''))
        with (folder / f'Photo-{index:04d}.png').open('xb') as output:
            output.write(data)
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.iterdir())}


def main():
    return runner.main(
        inventory=[{'id': IDENTIFIER, 'severity': 'P1', 'scope': 'CURRENT',
                    'mode': 'REGRESSION',
                    'scenario': 'Actual AppKit minimize during a two-item export queue; second '
                                'output completes while miniaturized; restore preserves window, '
                                'catalog, history, source bytes and exact output pixels.'}],
        executable_ids={IDENTIFIER}, fixture_factory=two_photos, domain='supplemental',
        default_report_name='ux-minimize-export-report',
    )


if __name__ == '__main__':
    raise SystemExit(main())
