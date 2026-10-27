# Edited thumbnails — 0.4.9 development

[Issue #27](https://github.com/yakut-sekonts/Apertone/issues/27) replaces original-camera
thumbnail previews with saved-edit renders. The shared calibrated CPU path applies
Develop, Curve/HSL, Noise/Sharpening, Optics, Transform, EXIF and Crop with Detail
preview scale. Grid/filmstrip update after successful autosave and batch apply/
Undo/Redo without selection, count or scroll resets. Hover/failed commits/cancelled
or no-op batch operations never invalidate saved thumbnails. PhotoCard retries
retain the model URL binding, including after a previous loading failure.

A dedicated SQLite read transaction snapshots parent edits, Curve/HSL children and
revision consistently. Disk identity hashes source checksum, saved revision and
canonical complete preset JSON, with an edited-pipeline version. Duplicate bytes
with different edits and catalogs reusing revision numbers cannot collide. Source
hashes are checked before/after decoding. Corrupt/oversized cache entries regenerate.
JPEG encoding is atomic; first and cached responses use the same encoded pixels.
Two workers, 96 admitted requests, 384px output and the existing 512MB cache remain.
No catalog/edit/preset schema changes. See ADR 0017.

A model URL generation nonce invalidates Qt image caching after commits. Only
cached affected rows are notified, with no full catalog scan or model reset; lazy
rows use the current nonce. Old requests retain independent disk identities and
Qt source replacement isolates late completions. Worker response completion is
queued on the response's QObject thread; cancellation/destruction remains bounded.

Validation includes calibrated generated Nikon DNG with edited geometry/EXIF,
JPEG tolerance against the shared render, exact cache/restart pixels, duplicate
source/reused-revision keys, corrupted cache, bounded requested size, concurrent
parent/Curve/HSL writes with atomic reads, actual QML late-old/new responses and
retry/new-URL recovery, autosave failure/retry/hover, batch apply/Undo/Redo/no-op,
selection/count preservation and untouched RAW hashes. Native Library acceptance
waits for the actual new PhotoCard image after autosave before closing.

Local validation passed: all 27 CTests including private Nikon restart,
clang-format and full/targeted clang-tidy. The thumbnail/snapshot/cache/QML/batch
acceptance also passed separately on the actual private Nikon D7500 NEF.
Exact-head macOS CI and standalone bundle validation remain integration gates. Advanced export/queue remain phase 0.4; no phase 0.5 work.
This is an ad-hoc development build, not a new notarized public release.
