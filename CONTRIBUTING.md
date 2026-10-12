# Contributing

Discuss a significant feature in an issue before implementation. Follow the
roadmap rather than adding unrelated controls. Keep core, platform code and UI
separate; record architectural changes in `ARCHITECTURE.md` and an ADR.

Use a feature branch and focused conventional commits (`feat:`, `fix:`, `test:`,
`perf:`, `refactor:`, `docs:`). The `main` branch must build and pass the required
CI checks. Significant changes use a PR with What, Why, Implementation, Testing
and Known limitations. Never claim an unimplemented control as a feature.

Run build, CTest, clang-format and clang-tidy before opening a PR. Image changes
need numerical tests and representative licensed/synthetic image coverage.
Persistence changes need restart and original-integrity tests. Do not commit
private photographs, credentials, closed SDKs or restricted camera/lens profiles.

Dependencies must be version-pinned, license-reviewed and recorded in
`THIRD_PARTY_LICENSES.md` before integration. Optional GPL components are not
enabled without a project-wide licensing decision.
