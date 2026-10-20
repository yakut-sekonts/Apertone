#!/usr/bin/env python3
"""Finite localized dialog slice; never full VIS007/VIS008 qualification."""
import run_ux_suite as runner
from run_keyboard_regression import one_png_fixture

IDENTIFIER = 'CANDIDATE-LOCALIZED-PRESET-DIALOGS'


def main():
    return runner.main(
        inventory=[{'id': IDENTIFIER, 'severity': 'P2', 'scope': 'CURRENT',
                    'mode': 'REGRESSION',
                    'scenario': 'One PNG and preset; Settings and preset Create/Edit/Copy '
                                'dialogs in eight languages at 1000x640; genuine controls, '
                                'text geometry, Cancel and persistence guards. '
                                'Not full VIS007/VIS008/native chooser coverage.'}],
        executable_ids={IDENTIFIER}, fixture_factory=one_png_fixture, domain='supplemental',
        default_report_name='ux-localized-dialog-report',
    )


if __name__ == '__main__':
    raise SystemExit(main())
