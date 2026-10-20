# Graphify for Luma

Graphify is an optional local development tool for navigating source relationships.
It is not a build dependency or a release quality gate.

## Installation

Use Python 3.12 and an isolated environment. The package name is `graphifyy`;
the executable is `graphify`. The inspected version is **0.9.92**.

```sh
python3.12 -m venv ~/.local/share/graphify/venv
~/.local/share/graphify/venv/bin/python -m pip install 'graphifyy==0.9.92'
~/.local/share/graphify/venv/bin/graphify install --platform codex
```

The official installer places the Codex skill and its references under
`~/.codex/skills/graphify/`. This setup needs no API key, hosted service,
background watcher or editor hook.

Upstream: <https://github.com/Graphify-Labs/graphify>.

## Local code graph

Run from the repository root:

```sh
~/.local/share/graphify/venv/bin/graphify extract . --code-only --no-dedup
~/.local/share/graphify/venv/bin/graphify diagnose multigraph
~/.local/share/graphify/venv/bin/graphify query 'ThumbnailProvider renderCPU' --budget 1500
~/.local/share/graphify/venv/bin/graphify explain 'EditorController::localHandles()'
```

`--code-only` uses local structural extraction and skips semantic model calls.
`--no-dedup` preserves distinct source entities instead of fuzzy-merging similarly
named symbols. `.gitignore` excludes generated builds and local QA evidence;
`.graphifyignore` excludes documentation and media from this code-focused corpus.
Output is written to the ignored `graphify-out/` directory.

For a report without model calls or an interactive visualization:

```sh
~/.local/share/graphify/venv/bin/graphify cluster-only . --no-label --no-viz
```

Re-run extraction after source changes before relying on the graph. The CLI can
update an existing graph incrementally; pass `--force` for a complete rebuild.
Use graph labels or their vocabulary for queries: the query engine does not
translate Russian questions or resolve synonyms automatically.

## Coverage limits

C++, Objective-C++, Python, shell and Swift source are supported by the installed
extractors. QML is not structurally supported in this version. Qt signal/slot
connections, dynamic dispatch, conditional compilation and runtime object
lifetimes still require source inspection and functional tests.

The first local scan on 2026-10-20 processed 347 code files and produced 5,764
nodes and 14,878 edges. Nine files had parser errors and 23 yielded no symbols;
these are extractor limitations, not compiler diagnostics. Graph structure
diagnostics found no dangling endpoints or exact duplicate edges. This checks
the graph format, not extraction completeness or application correctness.

The first review used `EditorController::localHandles()` to locate the
`CropGeometry` coverage check, then inspected `LocalOverlay.qml` directly for
the `raw.covered === false` proxy behavior. The thumbnail review required
reading both `LUMA_UX_THUMBNAIL_TRACE` and `LUMA_UX_CPU_RENDER_ROWS` branches:
the graph alone does not establish which renderer is shipped.

A path in the graph may represent containment or an include rather than a call.
Inspect each edge's relation, confidence and source location before using it to
explain behavior. A missing edge does not establish that no dependency exists.
Never use the graph to turn an unexecuted, failed or skipped test into a pass.
