# Ordinary image import and editing alongside RAW

Open photo and Library Add/Copy now accept JPEG/JPG, PNG, TIFF8/16, HEIC/HEIF,
WebP, BMP and GIF, plus AVIF where the native system decoder is available.
Full-image ICC conversion, EXIF orientation and photographic metadata feed the same
Develop, saved-edit thumbnail, preset/batch, queue and restoration paths as RAW.
The interface and all eight language packages use photo/image wording and filters.

Optional straight alpha survives preview, spatial processing, optics and geometry.
PNG/TIFF preserve coverage; JPEG/DNG flatten against black. The empty-alpha path
keeps existing RAW math and output. Header/completeness/budget checks and cooperative
cancellation reject damaged or oversized images. RAW TIFF recognition prevents an
embedded preview becoming the editing source, including for unsupported RAW layouts.
No catalog/edit migration or runtime codec library is added; originals remain hashed
and protected by existing atomic copy/export guards.

Validation: native ICC/grayscale/CMYK, all EXIF orientations, 16-bit ramps,
HEIC/WebP/GIF and optional AVIF fixtures, alpha through Detail/Optics/Transform and
all exports, invalid inputs/cancellation. Mixed-format acceptance covers Add/Copy,
duplicates, hashes, native Library/thumbnails, edit/export/restart, queue and RAW
TIFF routing. Full unit/integration/private Nikon acceptance, format/tidy,
byte-identical default RAW PNG against the actual 0.4.11 app, Nikon advanced export
and standalone bundle acceptance gate integration; exact-head macOS CI follows.

Source/development version 0.4.12 is the second requested pre-phase-0.5 extension.
Public v0.3.0 and the frozen 0.4.0 release candidate are unchanged. The development
bundle is ad-hoc signed, not notarized. Local Adjustments have not started.
See docs/IMAGE_SUPPORT.md and ADR 0020 for format semantics and limits.
