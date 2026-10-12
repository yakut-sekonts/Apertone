# Stage 0.2 review package

## Issue: 0.2 — Develop Core

Implement the next ROADMAP stage on top of M0: manual Temperature/Tint and WB
presets, Exposure/Contrast/Highlights/Shadows/Whites/Blacks, Vibrance/Saturation,
independent correction switches, gesture-based Undo/Redo and image export.

Acceptance: edit a RAW, export it, restart and reproduce the same pixels. Preserve
the original checksum and M0 catalog edits. Test numerical boundaries, migration,
history, destination integrity, cancellation and actual desktop restart.

## Pull request: feat: add Develop Core and full-resolution export

### What

Adds the first Develop toolset and full-resolution JPEG/PNG/TIFF16 export. Old
catalogs migrate transactionally to edit schema 2 without losing Exposure.
Undo/Redo, per-correction switches and resets use the same autosave/render path.

### Why

M0 supported Exposure only. This creates an end-to-end edit/export workflow before
the library and professional Develop/preset stages.

### Implementation

Retains demosaiced Float32 camera RGB with As Shot gains and camera calibration.
WB precedes camera-to-linear-sRGB conversion. A shared CPU pipeline applies tone
and color for both preview and export. Export rechecks source SHA-256, captures
an immutable edit document, writes through macOS ImageIO and atomically installs
a sibling temporary file. TIFF16 has no 8-bit intermediate. Explicit sRGB ICC
profiles are deterministic and independent of the current monitor.

### Testing

Five local CTest groups pass, including real desktop edits/Undo/Redo, migration
from M0, restart-identical PNGs, JPEG ICC, TIFF16, overwrite/hard-link protection,
cancel cleanup and close with queued operations. A private Nikon D7500 NEF also
passes full-resolution 5600×3728 PNG/TIFF16 export and restart-identical PNG with
an unchanged source checksum. Private images are not published. clang-format
and clang-tidy run with the existing project policy; public CI is verified on
the final published branch. The real QML screenshot uses a generated DNG.

### Known limitations

Reference WB/tone implementation; no production camera profiles or Lightroom
rendering-equivalence claim. History is session-local and capped at 200 operations.
Auto/eyedropper WB, persistent history/Versions, curves, detail, resize, alternative
output spaces, metadata options, sharpening, export queue and DNG remain planned.
The migration is forward-only; older M0 builds refuse a schema 2 catalog.

### Dependency

Based on the M0 implementation branch in PR #2; review only the stage 0.2 diff.
