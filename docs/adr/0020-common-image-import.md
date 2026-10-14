# 0020 — Common image ingestion through the shared Develop pipeline

Accepted for the human-requested pre-phase-0.5 extension, version 0.4.12, Issue #33.

Add JPEG/JPG, PNG, TIFF8/16, HEIC/HEIF, WebP, BMP and GIF to open/import/discovery.
AVIF is available when the macOS ImageIO decoder supports it. Use the primary image
for HEIF and the first image/frame/page for GIF/TIFF; this is still-image editing,
not animation or multi-page document authoring. Inspect native decoder capabilities
rather than claiming unavailable codecs. Keep RAW sensor decode in LibRaw; ImageIO
must never turn an embedded camera JPEG into the Develop source.

A shared PhotoDecoder dispatches RAW to the existing RawDecoder and supported
raster content to a macOS ImageIO adapter. Both return the same full-resolution
floating-point image/metadata/calibration payload. Native ICC conversion produces
straight linear sRGB, retaining 16-bit source precision and transparent
coverage. Untagged and grayscale/CMYK sources use their native decoder color
interpretation. Invalid, incomplete or oversized inputs fail explicitly before
unbounded allocation. Preserve source dimensions plus EXIF orientation and let the
existing geometry apply orientation exactly once.

FloatImage adds optional coverage (empty means fully opaque). Preserve exact RAW
behavior on the empty path. Downsampling, optics, geometry, noise reduction and
sharpening normalize samples by coverage so hidden transparent RGB cannot bleed
into visible edges. PNG/TIFF preserve coverage; JPEG/DNG flatten against black.
Export, editor preview, thumbnails, presets, catalog Add/Copy/recovery, queue and
restart share the decoded content. Original hashing/atomic destination protection
remain in force. No catalog/edit schema migration or new runtime codec library.

Validation uses project-generated ImageIO fixtures and small original lossless
WebP/animated-GIF fixtures with checksums and a reproducible generator. Cover ICC
reference colors, all eight EXIF orientations, metadata, transparency/spatial edges,
16-bit ramps, primary/first-frame behavior, malformed/unsupported/oversized inputs,
checksums/cancellation, mixed imports, GUI, queue/export/restart and unchanged RAW
outputs. Keep all eight 0.4.11 language packages current. Phase 0.5 remains on hold.
