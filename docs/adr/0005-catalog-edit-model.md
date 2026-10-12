# 0005 — Parameter edits linked to immutable originals
Status: Accepted for M0.

Context: The original must remain intact, including after missing-file and restart errors.

Decision: Asset UUID + canonical original path + SHA-256. Exposure lives in a
separate versioned edit row. Active selection is persisted. Reopening a changed
original fails safely while preserving its prior edit row.

Consequences: M0 has one current edit per asset. Named versions, history and
relocation are future migrations. No silent edit reassociation or original writes.
