# 0017 — Saved-edit thumbnails and independent cache identities

Status: accepted for phase 0.4, Issue #27.

Thumbnail workers render a bounded 384px preview with the same calibrated Develop,
Optics, Noise/Sharpening, Transform and Crop path as the editor. Embedded camera
JPEGs cannot represent current saved edits and are no longer used. Detail receives
the correct preview scale. JPEG thumbnails are opaque with black uncovered edges.
Two workers, 96 admitted requests and the existing 512MB atomic disk cache remain.
No phase-0.6 latency, full-resolution detail or large-catalog performance claim.

A fresh worker connection reads parent edits, Curve/HSL children and revision in
one explicit SQLite read transaction. Do not change assetByID, which is also used
inside batch write transactions. The disk identity hashes source checksum, saved
revision and canonical full preset JSON plus an edited-pipeline version. The edit
fingerprint prevents collisions between duplicate sources with different edits,
and when restored catalogs reuse revision numbers. Old original-v1 files cannot
satisfy new requests. Corrupted cached images are regenerated; writes stay atomic.

A UI generation nonce changes thumbnail URLs after successful autosave and batch
apply/Undo/Redo commits. Emit dataChanged only for affected cached rows (or cached
rows after batch), preserving selection, count and scroll. No full-catalog query,
model reset or per-photo in-memory map is needed. Lazy rows use the current nonce.
The nonce invalidates Qt image caching; disk keys use the actual saved snapshot.
Qt Image discards old responses after source replacement, while snapshot-specific
disk identities prevent old renders from overwriting a newer edit's cache entry.
Hover, unsaved gestures and failed commits never notify saved-thumbnail changes.

PhotoCard retries must remain part of the source binding and reset on a new model
URL. Imperative source assignment would detach that binding after the first error
and prevent later edits from updating the thumbnail.

Tests cover shared-render output, all relevant edit groups, source integrity,
consistent snapshots, different edits of identical source bytes, revision reuse,
corrupt cache, cancellation/destruction, old/new requests, QML source replacement,
autosave/model notifications, batch/Undo/Redo, selection and restart. No schema
migration, masks or phase-0.6 cache architecture is introduced.
