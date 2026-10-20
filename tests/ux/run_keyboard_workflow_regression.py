#!/usr/bin/env python3
"""Bounded keyboard import/edit/export; native chooser selection remains excluded."""
import run_ux_suite as runner
from run_keyboard_regression import one_png_fixture

IDENTIFIER = 'CANDIDATE-KEYBOARD-IMPORT-EDIT-EXPORT'


def main():
    return runner.main(
        inventory=[{'id': IDENTIFIER, 'severity': 'P2', 'scope': 'CURRENT',
                    'mode': 'REGRESSION',
                    'scenario': 'One photo: keyboard Import, selection, Develop, exposure and '
                                'PNG export. File-chooser result fixture only; no pointer input '
                                'in the application workflow. Not full native UX-KEY-015.'}],
        executable_ids={IDENTIFIER}, fixture_factory=one_png_fixture, domain='supplemental',
        default_report_name='ux-keyboard-workflow-report',
    )


if __name__ == '__main__':
    raise SystemExit(main())
