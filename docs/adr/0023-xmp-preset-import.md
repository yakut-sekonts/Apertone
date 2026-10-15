# 0023 — Sparse Camera Raw XMP preset import

The human requested .xmp preset import. Parse RDF/XML offline with Qt Core's
QXmlStreamReader; resolve expanded namespace URIs rather than prefix spellings.
Admit at most 1 MiB, 4096 nodes and depth 32. Reject DTD/entities, malformed XML,
duplicate properties and ambiguous Camera Raw descriptions before storage writes.

Convert recognized Light/Color, Curves, HSL and Detail fields to existing typed
settings. Read localized Name/Group with x-default priority, falling back to the
filename for a missing name. Unknown render settings are reported. Profiles,
embedded LUTs, masks, calibration, geometry and Adobe-only algorithms are not
executed or downloaded. Supported numeric values are not silently clamped.
The import report always explains that Luma's rendering can differ from Adobe.

XMP settings are sparse. A parameter selection travels with the preset through
storage, hover/apply, Undo, JSON export/reimport and immutable batch snapshots.
Only present properties and their correction switches change; unrelated values,
including controls within the same group, remain intact. Curve channels and the
white-balance mode have explicit selection tokens. Legacy native presets retain
whole-group semantics and schema 6. Sparse presets use schema 7; collections
containing one require schema 7 so older apps refuse rather than discard them.
No catalog/edit schema or image processing algorithms change.

References: [Adobe Camera Raw namespace](https://developer.adobe.com/xmp/docs/xmp-namespaces/crs/),
[Adobe's modern Camera Raw example](https://github.com/AdobeDocs/cis-photoshop-api-docs/blob/main/sample-code/lr-sample-app/crs.xml),
[Qt XML parser](https://doc.qt.io/qt-6/qxmlstreamreader.html).
