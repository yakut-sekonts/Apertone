# Ordinary photo/image support — 0.4.12

Open photo, drag-and-drop and Library Add/Copy accept JPEG/JPG, PNG, TIFF/TIF
(8/16 bit), HEIC/HEIF, WebP, BMP and GIF alongside the existing Bayer RAW formats.
AVIF is available when macOS ImageIO advertises its decoder. Folder discovery
checks extensions case-insensitively against installed decoder capabilities;
explicitly selected invalid files fail individually through the import journal.
No image codec download, account or new runtime dependency is required.

ImageIO decodes full pixels, converts embedded ICC profiles to the linear-sRGB
working space and retains source precision before Develop. Grayscale and CMYK
sources use native color conversion. Images without a profile use the native
format decoder's default color interpretation. HEIF uses its primary image;
GIF uses the first frame, TIFF the first page. Animation editing, multi-page
editing, HDR gain maps and auxiliary depth/matte data are outside this change.
EXIF orientation is applied exactly once by the shared geometry, including mirrors.
Available photographic EXIF fields populate capture data; missing fields stay empty.
As Shot preserves an ordinary image's existing source white balance. Explicit WB,
Curves/HSL, filters, Detail, Crop, Optics and Transform use the existing pipeline.

PNG/TIFF/WebP transparency is retained in previews and the working image.
Downsampling, spatial adjustments and geometry normalize by coverage to avoid
transparent-edge halos. PNG and TIFF export preserve alpha; JPEG and processed
Linear RGB DNG flatten against black. DNG remains processed output, not a way to
recover sensor RAW or the original editing latitude from a JPEG.

Library selection, edited thumbnails, presets, batch edits, queue and restoration
use the same decoder as the editor/CLI. Originals remain immutable: Add references
them, Copy verifies a separate byte-identical copy, and export refuses original
paths/hardlinks. Existing catalog/edit schemas are unchanged. Image headers are
checked before raster allocation: up to 20,000 px per side, 100 MP, and 512 MB
encoded input. Corrupt/truncated data fail explicitly; cancellation is cooperative
around native decode and during pixel conversion.

RAW sensor development stays in LibRaw. Recognized RAW TIFF containers always use
that path, including unsupported sensors which must fail rather than substitute
an embedded preview. Supported RAW layouts remain integer Bayer and Luma's own
identified processed Linear RGB DNG; this change does not expand camera support.

Tests include owned synthetic fixtures, all eight EXIF orientations, ICC reference
tones, grayscale/CMYK, 16-bit ramps, alpha edges through Detail/Optics/Transform,
invalid inputs/cancellation and all output formats. Mixed-format acceptance covers
Add/Copy/duplicates, original hashes, native Library/thumbnails, edit/export/restart,
queue and RAW TIFF routing. No private photographs are shipped in the fixtures.

See [ADR 0020](adr/0020-common-image-import.md) and [export options](EXPORT.md).
This is the second human-requested extension before phase 0.5, which remains on hold.
