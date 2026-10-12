# M0 review package

## Issue: M0 — RAW Vertical Slice

Implement the first milestone from `docs/REQUIREMENTS.md` §101 using the approved
C++20 / Qt / LibRaw / SQLite stack on macOS Apple Silicon.

Acceptance: clean clone → build → open Nikon D7500 NEF → inspect EXIF → change
Exposure → close → reopen → restore active photo and Exposure. Original SHA-256
must remain unchanged. CI must pass; public tests must not contain private RAW.

## Pull request: feat: implement M0 RAW vertical slice for macOS

### What

Adds a working Qt/QML RAW viewer, sensor-based Bayer decode, Exposure adjustment
and durable SQLite autosave/restoration. Establishes the public project structure,
stage roadmap, dependency pins, architecture ADRs and macOS CI.

### Why

This establishes the first complete non-destructive RAW workflow before adding
the broader Develop, library and preset features.

### Implementation

LibRaw provides integer sensor data and EXIF. Luma normalizes to Float32, applies
As Shot WB, bilinear demosaic and camera-to-linear-sRGB conversion. Exposure is
applied on a dedicated CPU preview lane; ColorSync/LittleCMS targets the monitor
ICC. A separate ordered job lane handles decoding and SQLite transactions. Normal
close flushes the debounced edit asynchronously. Desktop and CLI share services.

### Testing

Clean macOS arm64 build passed. Four local CTest groups passed: core numerical
and catalog tests, QML shell, synthetic DNG desktop/CLI restart, and genuine Nikon
D7500 NEF desktop/CLI restart. The private original's checksum remained unchanged.
Static analysis and the published GitHub CI result are verified separately.

### Known limitations

M0 supports integer Bayer only, uses reference bilinear demosaic and a Fit preview
of up to 2560 px. It does not include full Develop, grid, export, masks, presets,
undo or Metal pixel processing. Private Nikon coverage is skipped in public CI
when no licensed fixture is provided. The last committed edit survives a forced
kill; an uncommitted debounce interval is not guaranteed to survive.
