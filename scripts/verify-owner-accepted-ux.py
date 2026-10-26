#!/usr/bin/env python3
"""Verify that an incomplete UX run matches an explicit release-owner exception."""

import argparse
from collections import Counter
import json
from pathlib import Path
import sys
from typing import Optional


def fail(message: str) -> None:
    raise ValueError(message)


def exact_set(report: dict, exception: dict, report_key: str, acceptance_key: str) -> set[str]:
    actual = set(report.get(report_key, []))
    expected = set(exception.get(acceptance_key, []))
    if actual != expected:
        fail(f"{report_key} differs from the recorded owner exception: actual={sorted(actual)}, expected={sorted(expected)}")
    return actual


def verify(report: dict, acceptance: dict, runner_exit: int, expected_commit: Optional[str]) -> dict:
    if acceptance.get("decision") != "OWNER_ACCEPTED_WITH_EXCEPTIONS":
        fail("Acceptance decision is not OWNER_ACCEPTED_WITH_EXCEPTIONS")
    directions = acceptance.get("owner_directions", {})
    if directions.get("remaining_unadapted_qa_inventory") != "SKIPPED_BY_OWNER":
        fail("Remaining QA inventory is not explicitly skipped by the owner")
    if directions.get("installed_application_and_end_to_end_update_validation") != "SKIPPED_BY_OWNER":
        fail("Installed-app/update validation is not explicitly skipped by the owner")
    accounting = acceptance.get("accounting", {})
    if accounting.get("skipped_checks_are_pass") is not False:
        fail("Skipped checks must remain distinct from PASS")

    exception = acceptance.get("automated_gate_exception", {})
    if exception.get("status") != "SKIPPED_BY_OWNER":
        fail("Automated gate exception is not explicitly owner-approved")
    if runner_exit != exception.get("accepted_runner_exit"):
        fail(f"Runner exit {runner_exit} does not match the accepted incomplete exit")
    if report.get("execution_status") != exception.get("accepted_execution_status"):
        fail("Execution status differs from the recorded exception")
    if report.get("release_gate") != exception.get("accepted_release_gate"):
        fail("Release gate differs from the recorded exception")
    if report.get("source_consistency") != "PASS":
        fail("UX source changed during execution")
    if report.get("original_hashes") != "PASS":
        fail("An original fixture changed during execution")
    provenance = report.get("build_provenance", {})
    if provenance.get("status") != "PASS":
        fail("UX executable build provenance is not valid")
    observed_commit = report.get("hardware", {}).get("commit")
    if expected_commit and observed_commit != expected_commit:
        fail(f"UX report commit {observed_commit!r} does not match {expected_commit!r}")

    results = report.get("results")
    if not isinstance(results, list) or not results:
        fail("UX report has no results")
    forbidden = set(exception.get("forbidden_result_statuses", ["FAIL"]))
    forbidden_results = sorted(result.get("id", "<unknown>") for result in results
                               if result.get("status") in forbidden)
    if forbidden_results:
        fail(f"UX report contains forbidden results: {forbidden_results}")
    counts = Counter(result.get("status") for result in results)
    if dict(counts) != report.get("summary"):
        fail("UX summary does not match the result records")

    not_implemented = exact_set(report, exception, "not_implemented_applicable",
                                "accepted_not_implemented_applicable")
    deferred = exact_set(report, exception, "deferred_load_implemented",
                         "accepted_deferred_load_implemented")
    unsupported = exact_set(report, exception, "unsupported_backend",
                            "accepted_unsupported_backend")
    exact_set(report, exception, "unadapted_applicable", "accepted_unadapted_applicable")
    exact_set(report, exception, "policy_unreviewed", "accepted_policy_unreviewed")

    accepted_nonpassing = not_implemented | deferred | unsupported
    observed_nonpassing = {result.get("id") for result in results if result.get("status") != "PASS"}
    if observed_nonpassing != accepted_nonpassing:
        fail(f"Non-PASS results differ from the recorded owner exception: actual={sorted(observed_nonpassing)}, expected={sorted(accepted_nonpassing)}")
    if set(report.get("blocking_missing_or_failed", [])) != accepted_nonpassing:
        fail("Blocking incomplete IDs differ from the recorded owner exception")

    return {
        "status": "OWNER_EXCEPTION_VERIFIED",
        "commit": observed_commit,
        "pass": counts.get("PASS", 0),
        "skipped_by_owner": len(accepted_nonpassing),
        "release_gate_preserved_as": report.get("release_gate"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--acceptance", type=Path, required=True)
    parser.add_argument("--runner-exit", type=int, required=True)
    parser.add_argument("--expected-commit")
    args = parser.parse_args()
    try:
        result = verify(json.loads(args.report.read_text()),
                        json.loads(args.acceptance.read_text()),
                        args.runner_exit, args.expected_commit)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"Owner-accepted UX verification failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
