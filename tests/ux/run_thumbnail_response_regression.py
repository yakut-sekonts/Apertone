#!/usr/bin/env python3
"""Supplemental actual-response candidate; exact bounded PNG profile only."""
import argparse
import hashlib
import sys

import run_ux_suite as runner
from thumbnail_response_fixtures import palette_png

IDENTIFIER = 'CANDIDATE-LIB004-ACTUAL-RESPONSE'
INVENTORY = [{
    'id': IDENTIFIER, 'severity': 'P1', 'scope': 'CURRENT', 'mode': 'REGRESSION',
    'scenario': ('Actual current OLD Image.Loading/source/serial/revision before a real fast '
                 'scroll; a previously unrequested NEW visible response becomes Ready with '
                 'independent per-asset pixels while OLD successful completion is held; '
                 'release observes real OLD finished/cancel/destruction and intact data.'),
}]


def main():
    options_parser = argparse.ArgumentParser(add_help=False)
    options_parser.add_argument('--fixture-count', choices=(8, 12, 32), type=int, action='append')
    options, remaining = options_parser.parse_known_args()
    counts = options.fixture_count or [32]
    if len(counts) != 1:
        options_parser.error('--fixture-count must be supplied at most once')
    count = counts[0]
    def fixtures(folder):
        folder.mkdir(parents=True, exist_ok=True)
        if any(folder.iterdir()):
            raise ValueError('LIB004 fixture factory requires a fresh isolated directory')
        for index in range(count):
            palette_png(folder / f'Photo-{index:04}.png', index)
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in folder.glob('Photo-*.png')}
    sys.argv = [sys.argv[0], *remaining]
    return runner.main(inventory=INVENTORY, executable_ids={IDENTIFIER},
                       fixture_factory=fixtures, domain='supplemental',
                       default_report_name='ux-thumbnail-response-report')


if __name__ == '__main__':
    raise SystemExit(main())
