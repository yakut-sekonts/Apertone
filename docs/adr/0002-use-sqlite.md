# 0002 — SQLite catalog
Status: Accepted for M0.

Context: Edits must survive restart without touching RAW originals.

Decision: SQLite 3.50.4 amalgamation, WAL, synchronous FULL, foreign keys, explicit
migrations and transactions. Own a connection on each background operation.

Consequences: One writer lane gives predictable autosave ordering. Pending edits
are flushed before normal close. A forced kill only guarantees the last committed
transaction. Future schema versions are rejected safely.
