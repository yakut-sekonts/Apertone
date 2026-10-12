# ADR 0007: paged library and checkpointed import

Schema 3 adds import sessions/items, manual collections and membership indexes.
Develop schema 2 and source/edit identity remain unchanged. Upgrades recheck the schema under
BEGIN IMMEDIATE and apply all migrations in one transaction, avoiding startup
races. Already-current schema readers do not acquire a writer lock, so they
remain usable during WAL writes/imports.

The library model exposes the total row count but caches at most 16 pages of 128
metadata rows. SQL filtering/paging and mutations run on a worker. GridView and
ListView virtualize delegates. Selection is keyed by asset ID, independently of
the page cache; range/select-all resolves IDs on the worker. Basic filename,
path, camera/lens, rating, flag, label and collection filters precede pagination.

Add keeps sources in place; Copy creates a session-owned directory below the
selected destination. Copies use temporary siblings, streaming SHA-256 checks,
fsync and no-replace atomic installation. A per-file journal checkpoint records
the intended source/target/checksum before installation. Asset insertion and
item completion are one transaction. Cancel keeps completed items and removes
only the current owned temporary copy. Exact checksum/size duplicates are
skipped by default; filename/date are descriptive metadata, not proof of identity.

On startup only dead-owner sessions are recovered. Complete, matching copied
files can be registered; mismatching published files are preserved and reported.
Queued items are cancelled. Live-owner sessions are untouched. Recovery never
deletes original sources. Process-ID reuse can delay recovery; broader leases,
cross-host catalogs and automatic backups are stabilization work.

Thumbnails are generated lazily by a two-worker asynchronous provider, stored as
bounded sRGB JPEGs in a catalog-specific disk cache. Embedded previews are allowed
only in Library; unavailable previews fall back to the RAW reference pipeline.
Develop always decodes sensor data. Thumbnails show the original, not current
Develop corrections. Edited thumbnails, Move, advanced rename/date/backup import,
smart collections, XMP and large real-image performance certification are later work.
