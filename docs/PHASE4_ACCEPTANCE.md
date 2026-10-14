# Phase 0.4 acceptance — 2026-10-14

The human requested completion of phase 0.4 and a stop before phase 0.5. All
phase-0.4 implementation substages are complete: Curves/HSL/targeted tools, user
presets and atomic batch Sync/Paste, sharpening/manual noise reduction, Crop,
calibrated Optics, manual/Auto/Guided Transform, eight ready-made photographic
looks, saved-edit thumbnails, advanced export and sequential export queue.
No Local Adjustments, Metal, AI, merge or later stabilization work was started.

Advanced export implementation: `da038a2324576c5ea70f9d34980b9abb5027573d`.
The following local acceptance passed on the implemented code:

- All 29 CTest cases, including Nikon D7500 restart (49.35 seconds), clang-format,
  full clang-tidy with no analyzer errors, and whitespace checks.
- Independent ICC/reference colors, DNG tags/LibRaw reimport and linear samples,
  TIFF8/16 compression, alpha-aware resize, output sharpening, metadata/GPS and
  text/image watermark tests. Source encoding tags cannot override TIFF settings.
- Actual QML export controls, persisted reusable settings/restart/concurrent-write
  conflicts, queued edit/options snapshots, autosave and failed-autosave handling,
  per-file failures, skip/unique collisions, pause/resume/cancel and close/update.
- Real native application queue → processed DNG → CLI reimport using a generated
  Nikon-metadata Bayer DNG. Test images are generated; no private RAW is committed.
- Private Nikon D7500 full-resolution TIFF16 ZIP/ProPhoto, metadata privacy,
  output sharpening and Unicode watermark, exact output after restart, resize,
  processed DNG reimport and unchanged SHA-256 of the original.
- Default PNG bytes match the actual previous 0.4.9 application.
- Advanced PNG/TIFF8/TIFF16 retain projective transparency; JPEG/DNG flatten
  uncovered corners to black.
- Standalone 0.4.10 app with Qt and Sparkle SDK directories unavailable: Library,
  Develop, Optics, Transform/Filter/Undo, edited thumbnails, advanced queue/DNG,
  export/restart and original hashes. Bundle signature and ZIP SHA-256 verified.
- Frozen 0.4.0 candidate ZIP/DMG/BUILD_INFO SHA-256 remain unchanged.

The final documentation commit records these results without changing application
code. Repackaging records the exact final commit in BUILD_INFO.json. Public GitHub
integration is gated on successful push and PR macOS CI for that exact head.

The standalone 0.4.10 package is an ad-hoc signed development build without
notarization. Public v0.3.0 and the frozen 0.4.0 release candidate are unchanged.
DNG is processed LinearRaw, not camera-sensor archival; general RGB/float DNG
remains unsupported. Metadata excludes opaque MakerNotes/source previews and
source encoding/develop recipes. Export queue/history are session-local. See
[export behavior and limits](EXPORT.md) and the [completion checklist](PHASE4_CHECKLIST.md).

Stop here before phase 0.5; wait for the human's roadmap adjustments.
