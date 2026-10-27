# Advanced export settings and sequential queue

Photos can now export through a reusable desktop configuration: JPEG/PNG/TIFF8/
TIFF16 with None/LZW/ZIP and real processed Linear RGB DNG; sRGB/Display P3/
Adobe RGB/ProPhoto; seven resize modes, output sharpening, metadata/location
policies and text/image watermark. A local destination, filename template and
explicit Skip/Unique/Replace policy apply per file.

The queue waits for autosave, snapshots saved asset/edit/options and processes
one file at a time. It shows current filename, progress, completed/total and
per-file failures, continues after a failed job, pauses between files and supports
resume/cancel. Closing and updater restart wait for cancellation/cleanup. Later
edits/settings and image-watermark changes cannot alter planned results.

Source encoding tags are removed before metadata transfer so they cannot override
the chosen TIFF compression. Atomic fsynced output installation rechecks catalog
originals under a short writer transaction, including aliases and originals
registered during rendering. Existing default PNG bytes match the actual 0.4.9
application. No catalog/edit schema migration is needed.

Validation covers independent ICC/reference colors and DNG/LibRaw roundtrip;
TIFF depth/compression with source metadata; resize/alpha/HDR; output sharpening;
GPS/attribution and watermark orientation/opacity/Unicode; original aliases and
late registration; settings persistence/conflicts; immutable queue snapshots,
autosave failure, errors, pause/resume/cancel, close/update, actual export QML,
and native application queue → DNG reimport. Private Nikon D7500 validation covers
full-resolution TIFF16 ZIP/ProPhoto, metadata privacy, output sharpening/watermark,
exact restart and original hashes. Standalone packaging and exact-head macOS CI
are required integration gates.

DNG stores applied edits in 16-bit LinearRaw samples; it is not sensor archival.
Only Apertone's identified rendered RGB DNG calibration is added to ingestion; general
RGB/float DNG remains unsupported. Available photographic metadata excludes opaque
MakerNotes, embedded previews, sensor layout and source encoding/develop recipes.
Queue history is session-local. Development packaging remains ad-hoc signed and
not notarized; public v0.3.0 and the frozen 0.4.0 candidate are unchanged.

This completes the remaining requirements §52/53 of phase 0.4. Stop before phase
0.5 for the human's requested roadmap adjustments. See ADR 0018 and docs/EXPORT.md.
