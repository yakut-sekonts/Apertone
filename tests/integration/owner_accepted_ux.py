"""An owner exception may admit only the exact recorded incomplete UX inventory."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


VERIFIER = Path(__file__).resolve().parents[2] / "scripts/verify-owner-accepted-ux.py"


def fixtures():
    not_implemented = ["UX-NOT-001"]
    deferred = ["UX-LOAD-001"]
    unsupported = ["UX-NATIVE-001"]
    report = {
        "hardware": {"commit": "a" * 40},
        "build_provenance": {"status": "PASS"},
        "source_consistency": "PASS",
        "original_hashes": "PASS",
        "summary": {"PASS": 1, "NOT_IMPLEMENTED": 1, "SKIPPED": 2},
        "results": [
            {"id": "UX-PASS-001", "status": "PASS"},
            {"id": not_implemented[0], "status": "NOT_IMPLEMENTED"},
            {"id": deferred[0], "status": "SKIPPED", "skip_reason": "deferred_load"},
            {"id": unsupported[0], "status": "SKIPPED", "skip_reason": "unsupported_backend"},
        ],
        "execution_status": "INCOMPLETE",
        "release_gate": "NOT_QUALIFIED",
        "not_implemented_applicable": not_implemented,
        "deferred_load_implemented": deferred,
        "unsupported_backend": unsupported,
        "unadapted_applicable": [],
        "policy_unreviewed": [],
        "blocking_missing_or_failed": not_implemented + deferred + unsupported,
    }
    acceptance = {
        "decision": "OWNER_ACCEPTED_WITH_EXCEPTIONS",
        "owner_directions": {
            "remaining_unadapted_qa_inventory": "SKIPPED_BY_OWNER",
            "installed_application_and_end_to_end_update_validation": "SKIPPED_BY_OWNER",
        },
        "accounting": {"skipped_checks_are_pass": False},
        "automated_gate_exception": {
            "status": "SKIPPED_BY_OWNER",
            "accepted_runner_exit": 1,
            "accepted_execution_status": "INCOMPLETE",
            "accepted_release_gate": "NOT_QUALIFIED",
            "accepted_not_implemented_applicable": not_implemented,
            "accepted_deferred_load_implemented": deferred,
            "accepted_unsupported_backend": unsupported,
            "accepted_unadapted_applicable": [],
            "accepted_policy_unreviewed": [],
            "forbidden_result_statuses": ["FAIL"],
        },
    }
    return report, acceptance


class OwnerAcceptedUxTests(unittest.TestCase):
    def run_case(self, mutate=None, *, runner_exit=1, expected_commit="a" * 40):
        report, acceptance = fixtures()
        if mutate:
            mutate(report, acceptance)
        with tempfile.TemporaryDirectory(prefix="luma-owner-ux-") as temporary:
            root = Path(temporary)
            report_path = root / "report.json"
            acceptance_path = root / "acceptance.json"
            report_path.write_text(json.dumps(report))
            acceptance_path.write_text(json.dumps(acceptance))
            return subprocess.run([
                sys.executable, str(VERIFIER), "--report", str(report_path),
                "--acceptance", str(acceptance_path), "--runner-exit", str(runner_exit),
                "--expected-commit", expected_commit,
            ], capture_output=True, text=True, timeout=20)

    def test_exact_recorded_incomplete_inventory_is_accepted(self):
        result = self.run_case()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("OWNER_EXCEPTION_VERIFIED", result.stdout)

    def test_failure_or_unrecorded_gap_is_rejected(self):
        mutations = [
            lambda report, _acceptance: report["results"].append({"id": "UX-FAIL-001", "status": "FAIL"}),
            lambda report, _acceptance: report["not_implemented_applicable"].append("UX-NEW-001"),
            lambda report, _acceptance: report.update(source_consistency="CHANGED_DURING_SUITE"),
            lambda report, _acceptance: report["hardware"].update(commit="b" * 40),
        ]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                self.assertNotEqual(self.run_case(mutate).returncode, 0)
        self.assertNotEqual(self.run_case(runner_exit=2).returncode, 0)


if __name__ == "__main__":
    unittest.main()
