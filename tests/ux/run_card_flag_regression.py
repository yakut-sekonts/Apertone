#!/usr/bin/env python3
"""Bounded PhotoCard flag-caption regression; separate supplemental domain."""
import hashlib
from pathlib import Path

import run_ux_suite as runner

IDENTIFIER = 'CANDIDATE-PHOTOCARD-FLAG-LOCALIZATION'
INVENTORY = [{
    'id': IDENTIFIER, 'severity': 'P2', 'scope': 'CURRENT', 'mode': 'REGRESSION',
    'scenario': ('Two real PNG cards, actual metadata Flag controls and Settings language '
                 'choice: exact English/Russian and eight-locale Pick/Reject captions, '
                 'typed documents/history/durable flags/current thumbnails/source guards. '
                 'Not full VIS007/VIS008/LOC coverage.'),
}]


def two_png_fixture(folder):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    result = {}
    for index in range(2):
        source = folder / f'Photo-{index:04}.png'
        if source.is_symlink():
            raise ValueError('Card generated source cannot be a symbolic link')
        if not source.exists():
            runner.png(source, index)
        if not source.is_file():
            raise ValueError('Card generated source is not a regular file')
        result[str(source.resolve())] = hashlib.sha256(source.read_bytes()).hexdigest()
    return result


def main():
    return runner.main(inventory=INVENTORY, executable_ids={IDENTIFIER},
                       fixture_factory=two_png_fixture, domain='supplemental',
                       default_report_name='ux-card-flags-report')


if __name__ == '__main__':
    raise SystemExit(main())
