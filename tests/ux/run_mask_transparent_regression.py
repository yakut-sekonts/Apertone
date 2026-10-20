#!/usr/bin/env python3
"""Native-only transparent Radial handle regression; never full UX-MSK-023."""
import hashlib
from pathlib import Path
import struct
import zlib

import run_ux_suite as runner

IDENTIFIER = 'CANDIDATE-MSK023-TRANSPARENT-HANDLE'


def one_gray_png(folder):
    folder = Path(folder)
    if not folder.is_absolute() or folder.resolve() != folder or folder.exists():
        raise ValueError('Transparent-handle fixture requires a fresh canonical directory')
    folder.mkdir(parents=True, exist_ok=False)
    source = folder / 'Photo-0000.png'

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))

    pixels = b''.join(
        b'\0' + b''.join(bytes([32 + x % 17 * 3 + y % 11 * 2]) * 3 for x in range(256))
        for y in range(192)
    )
    encoded = (b'\x89PNG\r\n\x1a\n'
               + chunk(b'IHDR', struct.pack('>IIBBBBB', 256, 192, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))
    with source.open('xb') as output:
        output.write(encoded)
    return {str(source): hashlib.sha256(source.read_bytes()).hexdigest()}


def main():
    return runner.main(
        inventory=[{
            'id': IDENTIFIER, 'severity': 'P1', 'scope': 'CURRENT', 'mode': 'REGRESSION',
            'scenario': ('Actual Radial center motion then Scale50/ConstrainOff leaves a visible '
                         'radius over uncovered output; actual reacquire/move/release must preserve '
                         'independent geometry, full documents, natural revision/history/pixels/data.'),
        }],
        executable_ids={IDENTIFIER}, fixture_factory=one_gray_png, domain='supplemental',
        default_report_name='ux-mask-transparent-report',
    )


if __name__ == '__main__':
    raise SystemExit(main())
