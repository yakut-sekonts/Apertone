# 0018 — Explicit export settings, linear output and immutable queue jobs

Status: accepted for phase 0.4, Issue #30. Stop before phase 0.5.

Extend typed export options for JPEG/PNG/TIFF8/TIFF16 and processed Linear RGB DNG,
TIFF compression, sRGB/Display P3/Adobe RGB/ProPhoto, seven aspect-preserving resize
modes, output sharpening, metadata selection and text/image watermark. Defaults
retain the current exact full-resolution sRGB export path; new options use a
shared developed-linear geometry result, alpha-aware resize and output sharpening
before ICC output encoding. ICC profiles come from macOS named color spaces and
LittleCMS; do not introduce third-party profile redistribution or a new RAW SDK.

DNG must contain actual LinearRaw RGB samples with explicit calibration and a
neutral default rendering profile, not an image with a changed extension. This is
a processed export with edits baked into linear samples, distinct from camera
sensor archival. Originals remain unchanged. Validate TIFF/DNG tags and independent
LibRaw reimport/color behavior before integrating the writer or reader extension.
The writer stores a single D50 calibration using ICC PCS colorants, their inverse
ColorMatrix1 and an explicit ForwardMatrix1; AsShotNeutral is unity. A prototype
confirmed LibRaw exposes integer LinearRaw in color4_image and does not apply the
D50 forward matrix in rgb_cam. The reader extension therefore handles Luma's
identified rendered RGB DNG profile explicitly using the standard forward matrix,
D50→linear-sRGB transform and bounded integer samples. General RGB/float DNG and
multi-illuminant camera-profile interpretation remain unsupported. LibRaw rejects
RAW dimensions below 22px; DNG export validates that roundtrip boundary explicitly.
Use the primary Adobe specification and preserve its required technology notice:
"This product includes DNG technology under license by Adobe."

Metadata All copies source image metadata while normalizing output geometry and
encoding declarations. Copyright Only uses an explicit attribution allowlist;
Remove Location removes GPS and location-bearing IPTC/XMP fields; None retains
only format/color information required to interpret output. Watermarks composite
after resizing/sharpening with bounded opacity/size/placement and managed color.
Source encoding/bit-depth/compression/strip-layout tags are removed: retaining
them can override the requested TIFF codec in ImageIO. All destination writes
use temporary files, cancellation, fsync and atomic install under a short catalog
writer transaction, rechecking originals registered during rendering.
Guard catalog originals and application state, including cross-photo destinations.

An export controller plans immutable asset/edit/options snapshots after coordinating
pending autosave, then processes one file at a time. Queue progress identifies the
current file, completed/total and errors. Pause completes the current file before
waiting; Resume continues. Cancel stops the active file and pending jobs, preserving
completed outputs and cleaning incomplete temporary files. Close/update waits for
cancellation/cleanup. Edits after planning do not change queued results. Queue state
is session-local; reusable export settings are separately versioned/validated and
written atomically. Destination, naming and explicit conflict policies apply per
job and are rechecked during installation.

Acceptance covers exact old-default output, resize dimensions/alpha, independent
ICC/color and DNG reimport, TIFF compression/depth, output sharpening, metadata/
GPS privacy, watermarks, naming/collisions and source/state protection, reusable
settings, queue snapshots/errors/pause/resume/cancel/close, actual QML, private
Nikon full-resolution output and standalone app. Format/tidy, full tests and exact-
head macOS CI gate integration. No masks, Metal, AI or later roadmap work.

Primary references checked 2026-10-14:

- Adobe DNG 1.7.1.0: https://download.adobe.com/pub/adobe/dng/DNG_Spec_1_7_1_0.pdf
  (SHA256 abdecfd8e104e8b86cc054d3a40b677bb2ebe87131bfde080a1df3ee16eb2f8d).
- DNG resources and technology notice: https://www.adobe.com/support/downloads/dng/dng_sdk.html
- Apple named spaces: https://developer.apple.com/documentation/CoreGraphics/CGColorSpace
- Apple TIFF compression: https://developer.apple.com/documentation/imageio/kcgimagepropertytiffcompression
