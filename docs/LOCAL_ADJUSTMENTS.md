# Local Adjustments — 0.5.0

Develop → **Local adjustments** opens the local tools. Add a Linear gradient or
Radial gradient, then adjust local Exposure, Contrast, Highlights, Shadows, Whites,
Blacks, Vibrance and Saturation. A neutral new mask changes no pixels.

Drag a free part of the photo to redraw the selected gradient. Linear handles move
the start/end and the middle handle moves the whole gradient. Radial handles move
the center and resize its two axes; Rotation rotates the ellipse. Masks remain
anchored when Crop, EXIF orientation or Transform changes. Regions outside the
current crop can be edited by temporarily widening/resetting the crop.

Each mask has an editable name, enable state, Invert, Feather and Mask opacity.
The list order is processing order; the up/down controls reorder layers. Every
layer blends its adjustment with the preceding scene-linear result. These local
controls operate after global Develop/Detail, so their result is not an Adobe
Camera Raw rendering match or a second RAW white-balance calculation.

**Show mask overlay**, its three colors and Overlay opacity are viewing aids.
They do not alter masks, saved edits, export or Library thumbnails. The photo
shows the selected mask's actual feathered/inverted/opacity-weighted coverage.
The on-photo editor closes when entering Crop, Guided Transform, targeted tools,
Original, a preset preview, batch editing or export. Reopen **Edit mask on photo**
to continue. Every slider/pointer gesture is one Undo/Redo operation.

Masks are saved with the photo's edits and restored on restart. Export, including
advanced export and the immutable queue, and edited thumbnails use the shared CPU
mask evaluator. Global presets, XMP presets, Paste and Sync Settings preserve the
target photo's existing masks. Mask transfer via presets/batch is not part of 0.5.0.

## Storage and limits

Catalog schema 10 adds a mandatory versioned `edit_local_masks` document per photo;
global edit schema remains 8. Migration from catalog 9 adds neutral masks without
changing global settings or saved revisions. Original files are never modified.
Older Luma versions cannot open catalog 10: back up the catalog before upgrading
if you need to return to 0.4.x. Automatic catalog backup/recovery remains phase 0.9.

At most 128 masks per photo, 80 Unicode code points per name and 1 MiB serialized
mask data are admitted. Strict validation rejects unknown versions, invalid UTF-8,
non-finite/out-of-range values, duplicate IDs and incomplete/trailing data. Global
and local settings share a transaction; missing/corrupt mandatory mask data is
reported rather than silently reset. Batch Undo refuses conflicts with later mask
changes without overwriting those changes.

Mask coordinates are normalized pixel-edge coordinates in the optically corrected
source canvas, before Crop/Transform/EXIF. The UI inverse-maps the current preview;
full-resolution export evaluates the same normalized geometry. Preview/detail
resolution can still affect the underlying image; phase 0.6 owns true 1:1/Metal.

## Acceptance

- `local-mask-render-storage`: independently expected linear/radial coverage,
  feather/opacity/inversion/rotation, neutral/disabled exact pixels, ordered local
  Light/Color on signed HDR, all EXIF/quarter-turn/flip forward/inverse mappings,
  preview/linear-export alpha and color parity, overlay isolation, strict bounded
  serialization, migration 9→10, restart and failed migration rollback.
- `presets-storage-preview-history`: real LocalPanel slider and pointer events,
  one-gesture Undo/Redo, rename/reorder/flags/delete, radial redraw, deactivation,
  mutually exclusive tools, global Copy/Paste preservation and byte-identical
  export after restart.
- `local-mask-native-restart`: actual Main window, mask overlay with Transform,
  export/restart, original checksum and localized local panel in all eight packs.
- `edited-thumbnail-snapshot-cache`: shared saved-edit rendering includes masks;
  canonical mask bytes participate in the new `edited-v3` cache identity.
- Standalone bundle verification repeats local mask export/restart with Qt and
  Sparkle developer SDK directories unavailable.

Brush, color/luminance ranges and boolean composition remain subsequent phase-0.5
increments. Depth ranges require actual depth ingestion; 0.5.0 does not infer a
depth channel or provide AI masks. See ADR 0022 and Issue #40.
