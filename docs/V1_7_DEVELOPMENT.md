# Apertone 1.7 camera fidelity

Status: in development; not released.

## Highlight reconstruction

Integer Bayer RAW previously normalized every CFA site with LibRaw's common
`maximum`. Version 1.7 first uses a valid `linear_max[channel]` when the camera
metadata supplies one, including the separate second green channel. Missing or
invalid channel metadata falls back to the existing common maximum. Black-level
subtraction and black grids retain their previous behavior.

After edge-aware demosaic, a partially clipped camera-linear pixel can recover a
channel from the median of nearby, reliable target-to-anchor channel ratios. The
operation runs before White Balance, the camera-to-linear-sRGB matrix and all
editable corrections. It therefore produces one shared non-destructive source
for preview, thumbnails and export.

The recovery contract is deliberately bounded:

- pixels with no clipped channels are unchanged;
- fully clipped RGB pixels are unchanged because they have no chromatic anchor;
- estimates require at least three finite local ratios inside a 7 × 7 window;
- ratios and output are bounded to avoid unstable amplification;
- cancellation is checked per image row;
- the pass runs only when the sensor scan found a sample at or above its resolved
  channel white level;
- processing is in place, without a second full-resolution image allocation.

`DecodeDiagnostics` retains the common white level and additionally reports the
three resolved channel white levels and the recovered pixel count. The Develop
decode summary names camera highlight recovery only when at least one pixel was
actually reconstructed.

## Validation boundary

The changed image primitive has deterministic unit coverage for chromatic
recovery, fully clipped preservation, invalid samples and cancellation. The core
unit target and desktop application compile on the current macOS SDK.

Release qualification still requires representative licensed RAW photographs
from camera families whose metadata supplies different linearity limits, plus
comparison of preview and full-resolution export. These checks must confirm
highlight hue continuity without hiding genuinely clipped white regions. No
camera-specific quality claim is made before that evidence exists.
