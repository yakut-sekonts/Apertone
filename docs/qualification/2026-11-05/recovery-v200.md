# v2.0.0 recovery candidate

The annotated `v2.0.0` tag and its product commit remain immutable. This
candidate is a manual recovery workflow, not a successful rerun of the
original tag-triggered workflow. It rebuilds that commit with exactly the
single test-source change merged in PR #65, then restores the tagged test file
before packaging. The guarded source fingerprint and shipping-build hashes,
run ID, workflow commit, tag object and asset digests are recorded in
`RECOVERY_PROVENANCE.json`. The full set of original workflow build steps is
checked against the workflow stored in the tag and executed unchanged; the
owner-acceptance verifier is given the tag commit rather than the dispatch
workflow commit. Signing, package and updater metadata checks remain gates.

The publication workflow accepts only a successful recovery build on its same
reviewed main commit, verifies the artifact archive digests, and refuses an
existing release, including a draft. It does not move the tag or replace any
asset. `RECOVERY_PROVENANCE.json` is checked separately by GitHub's asset
SHA-256 digest and is not listed in the original `SHA256SUMS`. GitHub's create
release API can create a missing tag, and does not offer an atomic precondition
on the tag object: a server-side immutable-tag rule is required to eliminate a
race with external tag deletion or replacement. The script checks the annotated
tag object immediately before creation and again before publication, but those
checks cannot substitute for such a rule. This candidate does not in itself demonstrate a successful original
tag-triggered workflow, nor the installed-app end-to-end update or any other
owner-skipped scenario. Any such outstanding release requirements need their
own evidence or explicit owner decision before publication.

The packaged-app verifier had a stale schema expectation of 22; the tagged
application and integration tests use schema 23. The recovery changes only this
assertion immediately before the unchanged verifier step, restores the tagged
file afterwards, and records both exact verifier digests in provenance. The
publisher requires successful verification and the pinned digests.
