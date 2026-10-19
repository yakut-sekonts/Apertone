#!/usr/bin/env python3
"""Supplemental keyboard activation regression; never full UX-KEY-015 coverage."""
import hashlib
from pathlib import Path

import run_ux_suite as runner

# Retain the original identifier for existing imports and --case invocations.
IDENTIFIER = 'CANDIDATE-KEYBOARD-SELECTED-ASSET-ACTIVATION'
KEYPAD_IDENTIFIER = 'CANDIDATE-KEYBOARD-KEYPAD-ASSET-ACTIVATION'
IDENTIFIERS = frozenset((IDENTIFIER, KEYPAD_IDENTIFIER))
INVENTORY = [{
    'id': IDENTIFIER, 'severity': 'P2', 'scope': 'CURRENT', 'mode': 'REGRESSION',
    'scenario': ('Selected imported asset opens through bounded actual Tab/Shift+Tab and '
                 'Space/Return routes. Declared pointer import fixture; native chooser and '
                 'full UX-KEY-015 coverage are excluded.'),
}, {
    'id': KEYPAD_IDENTIFIER, 'severity': 'P2', 'scope': 'CURRENT', 'mode': 'REGRESSION',
    'scenario': ('Imported asset opens through bounded actual Tab/Shift+Tab and keypad '
                 'Enter with Qt.KeypadModifier. Declared pointer import fixture; native '
                 'chooser and full UX-KEY-015 coverage are excluded.'),
}]


def one_png_fixture(folder):
    """Use one generated standard PNG; never request unrelated stress fixtures."""
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'Photo-0000.png'
    if source.is_symlink():
        raise ValueError('Supplemental generated source cannot be a symbolic link')
    if not source.exists():
        runner.png(source, 0)
    if not source.is_file():
        raise ValueError('Supplemental generated source is not a regular file')
    return {str(source.resolve()): hashlib.sha256(source.read_bytes()).hexdigest()}


def main():
    return runner.main(inventory=INVENTORY, executable_ids=IDENTIFIERS,
                       fixture_factory=one_png_fixture, domain='supplemental',
                       default_report_name='ux-keyboard-report')


if __name__ == '__main__':
    raise SystemExit(main())
