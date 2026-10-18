"""Mocked flag/record contracts only; no native process or full-document oracle proof."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stderr
from unittest.mock import patch

import history_chaos_cli as cli


def args(**overrides):
    values = dict(seed=None, actions=None, replay=None, suite=None, case=[], phase=None,
                  release_qualification=False)
    return argparse.Namespace(**{**values, **overrides})


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode() + b'\n'


def record(path):
    # Structurally valid mock only. Native regeneration must reject this made-up hash chain.
    header = dict(kind='header', schema=1, case=cli.IDENTIFIER, model=cli.MODEL,
                  generator=cli.GENERATOR, seed='0', actions=1, document_schema=8,
                  application_version='fixture', qt_version='fixture',
                  fixture_sha256=['a' * 64] * 10, initial_hash='0' * 64)
    action = dict(kind='action', index=1, op='edit', control='contrast', delta=1,
                  before='0' * 64, after='1' * 64, undo=1, redo=0, changed=True)
    prefix = encoded(header) + encoded(action)
    footer = dict(kind='footer', actions=1, prefix_sha256=hashlib.sha256(prefix).hexdigest(),
                  final_hash='1' * 64, undo=1, redo=0)
    path.write_bytes(prefix + encoded(footer))


class RoutingTests(unittest.TestCase):
    def configure(self, value):
        with redirect_stderr(io.StringIO()):
            cli.configure_history_chaos(value, argparse.ArgumentParser())
        return value

    def test_default_count_and_seed_are_preserved_exactly(self):
        value = self.configure(args(suite='chaos', case=[cli.IDENTIFIER]))
        self.assertEqual((value.seed, value.actions), (123456, 10000))
        value = self.configure(args(suite='chaos', case=[cli.IDENTIFIER], seed=0, actions=10001))
        self.assertEqual((value.seed, value.actions), (0, 10001))

    def test_unknown_scope_and_silently_unused_flags_are_rejected(self):
        invalid = [args(suite='chaos'), args(suite='chaos', case=['UX-APP-020']),
                   args(suite='regression', seed=1), args(case=['UX-NAV-001'], actions=10),
                   args(case=[cli.IDENTIFIER], suite='blocking'),
                   args(case=[cli.IDENTIFIER], phase='0.5'), args(seed=-1),
                   args(seed=2**64), args(actions=0), args(actions=100001),
                   args(actions=9999, release_qualification=True)]
        for value in invalid:
            with self.subTest(value=vars(value)), self.assertRaises(SystemExit) as error:
                self.configure(value)
            self.assertEqual(error.exception.code, 2)

    def test_replay_adopts_metadata_only_when_not_explicitly_conflicting(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'replay.jsonl'
            record(path)
            value = self.configure(args(replay=path))
            self.assertEqual((value.seed, value.actions, value.suite, value.case),
                             (0, 1, 'chaos', [cli.IDENTIFIER]))
            for override in (dict(seed=1), dict(actions=2), dict(suite='all'),
                             dict(case=['UX-APP-020']), dict(phase='0.5'),
                             dict(release_qualification=True)):
                with self.subTest(override=override), self.assertRaises(SystemExit) as error:
                    self.configure(args(replay=path, **override))
                self.assertEqual(error.exception.code, 2)

    def test_replay_bounds_actual_read_if_file_grows_after_stat(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'replay.jsonl'
            record(path)
            original = path.read_bytes()
            # Real pre-read stat stays below this tiny mocked limit; the opened
            # stream models growth between stat and read, without a huge fixture.
            with patch.object(cli, 'MAX_REPLAY_BYTES', len(original) + 2), \
                    patch.object(Path, 'open', return_value=io.BytesIO(original + b'grew-after-stat')):
                with self.assertRaises(SystemExit) as error:
                    self.configure(args(replay=path))
                self.assertEqual(error.exception.code, 2)

    def test_replay_rejects_truncation_checksum_unknown_keys_and_boolean_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'replay.jsonl'
            record(path)
            original = path.read_bytes()
            malformed = [original[:-1], original.replace(b'"schema":1', b'"schema":true'),
                         original.replace(b'"actions":1', b'"actions":true'),
                         original.replace(b'"kind":"header"', b'"extra":0,"kind":"header"'),
                         original.replace(b'"delta":1', b'"delta":2')]
            for contents in malformed:
                path.write_bytes(contents)
                with self.subTest(contents=contents[:80]), self.assertRaises(SystemExit) as error:
                    self.configure(args(replay=path))
                self.assertEqual(error.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
