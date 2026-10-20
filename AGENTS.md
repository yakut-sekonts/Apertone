# Luma development

- Respond in Russian; preserve code identifiers and technical terms.
- Use Graphify to inspect code relationships and the impact of a proposed change.
  See `docs/development/GRAPHIFY.md` for the local setup and commands.
- Treat the graph as a navigation aid. Verify findings in source and appropriate
  tests; graph edges are not proof of runtime behavior or test coverage.
- Graphify 0.9.92 does not structurally parse QML. Inspect QML bindings, signal
  connections and UI behavior separately.
- Keep generated graphs and caches out of Git and application dependencies.
- User direction, 2026-10-20: implement the remaining roadmap toward 1.0.0
  without intermediate builds, test runs, static-analysis or QA campaigns.
  Record reported defects in `docs/DEVELOPMENT_BACKLOG.md`.
  Run the complete functional, image-quality, UX, load, stress, recovery and
  performance qualification before the final release; fix and retest then.
  Do not publish intermediate development builds or mark deferred checks PASS.
  Keep the existing release gates and historical failure evidence.
