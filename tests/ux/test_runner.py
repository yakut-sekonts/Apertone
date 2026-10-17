"""Test release-gate accounting and process supervision, not application UX."""
import argparse
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('ux_runner', Path(__file__).with_name('run_ux_suite.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class GateTests(unittest.TestCase):
    inventory = [{'id': 'A', 'mode': 'BLOCKING', 'scope': 'CURRENT'},
                 {'id': 'B', 'mode': 'BLOCKING', 'scope': '0.6'}]

    def test_missing_failed_and_incomplete_repetitions_block(self):
        for statuses in ([], ['PASS'], ['PASS', 'PASS'], ['FAIL', 'PASS', 'PASS'], ['NOT_IMPLEMENTED'] * 3,
                         ['SKIPPED'] * 3, ['FLAKY'] * 3):
            with self.subTest(statuses=statuses):
                rows = [{'id': 'A', 'status': status} for status in statuses]
                self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'FAIL')

    def test_selection_does_not_claim_release_qualification(self):
        rows = [{'id': 'A', 'status': 'PASS'}] * 3
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', False)[0], 'NOT_QUALIFIED')
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'PASS')
        self.assertEqual(runner.qualify(rows, self.inventory, '0.6', True)[0], 'FAIL')

    def test_subset_modes_never_claim_full_release_qualification(self):
        args = argparse.Namespace(release_qualification=True, case=[], suite='all', phase=None)
        self.assertTrue(runner.qualification_requested(args))
        for field, value in [('case', ['UX-APP-001']), ('suite', 'blocking'),
                             ('suite', 'regression'), ('phase', '0.5')]:
            with self.subTest(field=field, value=value):
                subset = argparse.Namespace(**{**vars(args), field: value})
                self.assertFalse(runner.qualification_requested(subset))

    def test_inventory_has_unique_ids(self):
        identifiers = [row['id'] for row in runner.INVENTORY]
        self.assertEqual(len(identifiers), len(set(identifiers)))
        self.assertEqual(len(identifiers), 770)

    def test_ci_output_paths_follow_build_and_preserve_explicit_overrides(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / 'build'
            build.mkdir()
            snapshot = {'source_tree_sha256': 'fixture', 'test_executable_sha256': 'fixture'}
            for explicit in (False, True):
                with self.subTest(explicit=explicit):
                    artifacts = root / 'custom-evidence' if explicit else build / 'ux-artifacts'
                    report = root / 'custom-report' if explicit else build / 'ux-report'
                    argv = ['run_ux_suite.py', '--build', str(build), '--suite', 'blocking']
                    if explicit:
                        argv += ['--artifacts', str(artifacts), '--report', str(report)]
                    opened = []
                    original_open = Path.open
                    def record_open(path, *args, **kwargs):
                        file = original_open(path, *args, **kwargs)
                        opened.append(file)
                        return file
                    # CLI routing test: no native process, fixture generation or screen ownership.
                    with (patch.object(runner, 'ROOT', root),
                          patch.object(runner, 'source_phase', return_value='0.5'),
                          patch.object(runner, 'INVENTORY', []),
                          patch.object(runner, 'fixtures', return_value={}) as fixtures,
                          patch.object(runner, 'fingerprint', return_value=snapshot),
                          patch.object(runner, 'build_provenance', return_value={'status': 'PASS'}),
                          patch.object(runner, 'write_reports') as reports,
                          patch.object(runner.fcntl, 'flock'),
                          patch.object(Path, 'open', record_open),
                          patch('sys.argv', argv), patch('builtins.print')):
                        self.assertEqual(runner.main(), 1)  # An empty suite never qualifies.
                        self.assertEqual(reports.call_args.args[1], report)
                        self.assertEqual(fixtures.call_args.args[0], artifacts / 'public-fixtures')
                        self.assertTrue(opened)
                        self.assertTrue(all(file.closed for file in opened),
                                        'Runner retained its screen lock after returning')

    def test_source_phase_tracks_the_build_and_rejects_unknown_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            configuration = root / 'CMakeLists.txt'
            configuration.write_text('project(Luma VERSION 0.10.7 LANGUAGES C CXX OBJCXX)')
            with patch.object(runner, 'ROOT', root):
                self.assertEqual(runner.source_phase(), '0.10')
                configuration.write_text('project(Unknown LANGUAGES CXX)')
                with self.assertRaises(ValueError):
                    runner.source_phase()

    def test_phase_order_is_numeric_and_target_is_never_current(self):
        self.assertTrue(runner.in_scope('0.6', '0.10'))
        self.assertFalse(runner.in_scope('0.10', '0.6'))
        self.assertFalse(runner.in_scope('TARGET', '1.0'))
        self.assertTrue(runner.in_scope('ALL', '0.5'))

    def test_nonblocking_polish_does_not_replace_the_severity_gate(self):
        rows = [{'id': 'A', 'status': 'PASS'}] * 3
        rows.append({'id': 'polish', 'status': 'FAIL', 'severity_if_failed': 'P2'})
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'PASS')
        rows[-1]['severity_if_failed'] = 'P1'
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'FAIL')

    def test_unadapted_current_regression_cannot_qualify_a_full_run(self):
        rows = [{'id': 'A', 'status': 'PASS'}] * 3
        rows.append({'id': 'regression', 'status': 'SKIPPED', 'scope': 'CURRENT',
                     'severity': 'P2', 'skip_reason': 'unadapted'})
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'FAIL')
        rows[-1]['skip_reason'] = 'hardware'
        self.assertEqual(runner.qualify(rows, self.inventory, '0.5', True)[0], 'PASS')

    def test_omitted_current_nonblocking_result_cannot_qualify(self):
        rows = [{'id': 'A', 'status': 'PASS'}] * 3
        inventory = self.inventory + [{'id': 'regression', 'scope': 'CURRENT', 'mode': 'REGRESSION'}]
        self.assertEqual(runner.qualify(rows, inventory, '0.5', True)[0], 'FAIL')

    def test_build_provenance_requires_both_matching_source_and_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            snapshot = {'source_tree_sha256': 'source', 'test_executable_sha256': 'binary'}
            self.assertEqual(runner.build_provenance(root, snapshot)['status'], 'UNVERIFIED')
            path = root / 'ux-build-provenance.json'
            path.write_text(json.dumps({'schema': 1, **snapshot}))
            self.assertEqual(runner.build_provenance(root, snapshot)['status'], 'PASS')
            for field in snapshot:
                changed = {**snapshot, field: 'changed'}
                self.assertEqual(runner.build_provenance(root, changed)['status'], 'UNVERIFIED')
            path.write_text('[]')
            self.assertEqual(runner.build_provenance(root, snapshot)['status'], 'UNVERIFIED')
            path.write_text('corrupt')
            self.assertEqual(runner.build_provenance(root, snapshot)['status'], 'UNVERIFIED')

    def test_phase_selection_keeps_prior_blocking_regressions(self):
        for scope in ('ALL', 'CURRENT', '0.5', '0.6'):
            self.assertTrue(runner.phase_selected({'scope': scope, 'mode': 'BLOCKING'}, '0.6'))
        self.assertFalse(runner.phase_selected({'scope': '0.7', 'mode': 'BLOCKING'}, '0.6'))
        self.assertFalse(runner.phase_selected({'scope': '0.5', 'mode': 'REGRESSION'}, '0.6'))
        self.assertTrue(runner.phase_selected({'scope': '0.6', 'mode': 'REGRESSION'}, '0.6'))

    def test_scenario_deadlines_preserve_explicit_override(self):
        defaults = argparse.Namespace(timeout=None)
        self.assertEqual(runner.scenario_timeout('UX-NAV-001', defaults), 180)
        self.assertEqual(runner.scenario_timeout('UX-UND-016', defaults), 600)
        self.assertEqual(runner.scenario_timeout('UX-ADV-013', defaults), 600)
        self.assertEqual(runner.scenario_timeout('UX-PRS-019', defaults), 900)
        self.assertEqual(runner.scenario_timeout('UX-IMP-019', defaults), 600)
        self.assertEqual(runner.scenario_timeout('UX-ADV-012', defaults), 600)
        self.assertEqual(runner.scenario_timeout('UX-NAV-002', defaults), 300)
        for value in (1, 17, 1200):
            override = argparse.Namespace(timeout=value)
            for identifier in ('UX-NAV-001', 'UX-UND-016', 'UX-PRS-019'):
                self.assertEqual(runner.scenario_timeout(identifier, override), value)

    def test_release_repeats_blockers_without_tripling_default_soaks(self):
        args = argparse.Namespace(release_qualification=True, runs=1)
        self.assertEqual(runner.repetitions_for({'mode': 'BLOCKING'}, args), 3)
        self.assertEqual(runner.repetitions_for({'mode': 'SOAK'}, args), 1)
        self.assertEqual(runner.repetitions_for({'mode': 'REGRESSION'}, args), 1)
        args.runs = 5
        self.assertEqual(runner.repetitions_for({'mode': 'BLOCKING'}, args), 5)
        self.assertEqual(runner.repetitions_for({'mode': 'SOAK'}, args), 5)

    def test_original_mutation_and_integrity_only_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'Original.png'
            source.write_bytes(b'original')
            baseline = {str(source): runner.hashlib.sha256(source.read_bytes()).hexdigest()}
            self.assertEqual(runner.changed_originals(baseline), [])
            source.write_bytes(b'changed')
            self.assertEqual(runner.changed_originals(baseline), [str(source)])
            source.unlink()
            self.assertEqual(runner.changed_originals(baseline), [str(source)])
            summary = {'hardware': {'commit': 'fixture', 'worktree_dirty': True,
                                   'source_tree_sha256': 'fixture', 'test_executable_sha256': 'fixture'},
                       'execution_status': 'FAIL', 'release_gate': 'FAIL', 'summary': {'FAIL': 1},
                       'original_hashes': 'FAIL', 'blocking_missing_or_failed': ['A'],
                       'results': [{'id': 'ORIGINAL-INTEGRITY', 'status': 'FAIL', 'severity': 'P0'}]}
            runner.write_reports(summary, root)
            self.assertIn('ORIGINAL-INTEGRITY', (root / 'UX_REPORT.md').read_text())
            self.assertIn('Reproduced failures', (root / 'ROADMAP_CANDIDATES.md').read_text())

    def test_zero_exit_cannot_hide_catalog_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / 'luma_ux_tests'
            child.write_text('#!/usr/bin/env python3\nimport json,sys\nfrom pathlib import Path\nr=Path(sys.argv[sys.argv.index("--root")+1])\n(r/"catalog.sqlite").write_bytes(b"Corrupted database")\n(r/"result.json").write_text(json.dumps({"id":"A","status":"PASS"}))\n')
            child.chmod(0o755)
            connections = []
            original_connect = sqlite3.connect
            def record_connection(*args, **kwargs):
                connection = original_connect(*args, **kwargs)
                connections.append(connection)
                return connection
            with patch.object(runner.sqlite3, 'connect', record_connection):
                result = runner.execute({'id': 'A', 'scope': 'CURRENT', 'mode': 'BLOCKING',
                                         'severity': 'P0', 'scenario': 'Corrupt fake child'},
                                        root / 'result', argparse.Namespace(build=root, timeout=10), root)
            self.assertEqual(result['status'], 'FAIL')
            self.assertIn('P0 catalog integrity', result['message'])
            self.assertTrue(connections)
            for connection in connections:
                with self.assertRaises(sqlite3.ProgrammingError):
                    connection.cursor()

    def test_watchdog_overrides_a_forged_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / 'luma_ux_tests'
            child.write_text('#!/usr/bin/env python3\nimport json,sys,time\nfrom pathlib import Path\nr=Path(sys.argv[sys.argv.index("--root")+1])\n(r/"result.json").write_text(json.dumps({"id":"A","status":"PASS"}))\ntime.sleep(30)\n')
            child.chmod(0o755)
            result = runner.execute({'id': 'A', 'scope': 'CURRENT', 'mode': 'BLOCKING',
                                     'severity': 'P0', 'scenario': 'Hung fake child'},
                                    root / 'result', argparse.Namespace(build=root, timeout=15), root)
            self.assertEqual(result['status'], 'FAIL')
            self.assertIn('P0 hard hang', result['message'])

    def test_restart_uses_new_process_and_cannot_hide_second_stage_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / 'luma_ux_tests'
            child.write_text('''#!/usr/bin/env python3
import json,sys
from pathlib import Path
r=Path(sys.argv[sys.argv.index("--root")+1])
stage=sys.argv[sys.argv.index("--stage")+1]
state=Path(sys.argv[sys.argv.index("--state-root")+1])
state.mkdir(exist_ok=True)
if stage == "prepare":
    (state/"saved.txt").write_text("durable")
else:
    assert (state/"saved.txt").read_text() == "durable"
(r/"result.json").write_text(json.dumps({"id":"UX-APP-003","status":"PASS" if stage == "prepare" else "FAIL","message":stage}))
''')
            child.chmod(0o755)
            result = runner.execute_scenario({'id': 'UX-APP-003', 'scope': 'CURRENT', 'mode': 'REGRESSION',
                                             'severity': 'P1', 'scenario': 'Fake restart'},
                                            root / 'result', argparse.Namespace(build=root, timeout=10), root)
            self.assertEqual(result['status'], 'FAIL')
            self.assertEqual(len(result['stages']), 2)
            self.assertNotEqual(result['stages'][0]['child_pid'], result['stages'][1]['child_pid'])

    def test_updater_checks_all_three_operations_in_distinct_processes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / 'luma_ux_tests'
            child.write_text('#!/usr/bin/env python3\nimport json,sys\nfrom pathlib import Path\nr=Path(sys.argv[sys.argv.index("--root")+1])\nvariant=sys.argv[sys.argv.index("--variant")+1]\n(r/"result.json").write_text(json.dumps({"id":"UX-UPD-003","status":"FAIL" if variant == "batch" else "PASS","message":variant}))\n')
            child.chmod(0o755)
            result = runner.execute_scenario({'id': 'UX-UPD-003', 'scope': 'CURRENT', 'mode': 'BLOCKING',
                                             'severity': 'P1', 'scenario': 'Fake updater operations'},
                                            root / 'result', argparse.Namespace(build=root, timeout=10), root)
            self.assertEqual(result['status'], 'FAIL')
            self.assertEqual([stage['message'] for stage in result['stages']], ['import', 'export', 'batch'])
            self.assertEqual(len({stage['child_pid'] for stage in result['stages']}), 3)

    def test_invalid_kill_checkpoint_never_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / 'luma_ux_tests'
            child.write_text('''#!/usr/bin/env python3
import json,sys,time
from pathlib import Path
r=Path(sys.argv[sys.argv.index("--root")+1])
(r/"kill-ready.json").write_text(json.dumps({"id":"UX-UND-014","stage":"prepare","pid":-1}))
time.sleep(30)
''')
            child.chmod(0o755)
            result = runner.execute({'id': 'UX-UND-014', 'scope': 'CURRENT', 'mode': 'BLOCKING',
                                     'severity': 'P0', 'scenario': 'Invalid fake marker'},
                                    root / 'result', argparse.Namespace(build=root, timeout=10, stage='prepare'), root)
            self.assertEqual(result['status'], 'FAIL')
            self.assertEqual(result['message'], 'Invalid intentional-kill stage marker')


if __name__ == '__main__':
    unittest.main()
