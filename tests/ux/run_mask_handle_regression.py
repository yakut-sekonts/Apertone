#!/usr/bin/env python3
"""Bounded Crop/gradient handle regressions; never full UX-MSK-023 coverage."""
import run_ux_suite as runner
from run_keyboard_regression import one_png_fixture

IDENTIFIER = 'CANDIDATE-MSK023-CROP-LOSS'
THIN_IDENTIFIER = 'CANDIDATE-MSK023-CROP-LOSS-THIN'
IDENTIFIERS = frozenset((IDENTIFIER, THIN_IDENTIFIER))
INVENTORY = [{
    'id': identifier, 'severity': 'P1', 'scope': 'CURRENT', 'mode': 'REGRESSION',
    'scenario': (f'{label} Crop/Done: all six Linear/Radial handle gestures in '
                 'base/H/HV/V states preserve independent geometry, complete documents, '
                 'history, current pixels, natural persistence and original bytes. '
                 'The thin variant also rejects ordinary input in photo padding.'),
} for identifier, label in ((IDENTIFIER, 'Normal'), (THIN_IDENTIFIER, 'Thin expanded-overlay'))]


def main():
    return runner.main(inventory=INVENTORY, executable_ids=IDENTIFIERS,
                       fixture_factory=one_png_fixture, domain='supplemental',
                       default_report_name='ux-mask-handles-report')


if __name__ == '__main__':
    raise SystemExit(main())
