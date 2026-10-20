# Luma development

- Respond in Russian; preserve code identifiers and technical terms.
- Use Graphify to inspect code relationships and the impact of a proposed change.
  See `docs/development/GRAPHIFY.md` for the local setup and commands.
- Treat the graph as a navigation aid. Verify findings in source and appropriate
  tests; graph edges are not proof of runtime behavior or test coverage.
- Graphify 0.9.92 does not structurally parse QML. Inspect QML bindings, signal
  connections and UI behavior separately.
- Keep generated graphs and caches out of Git and application dependencies.
- Run functional and quality checks before publishing a stable release.
  Load, stress, soak and performance testing remain deferred until pre-1.0.0.
  Keep failed and skipped results explicit; never count them as passes.
