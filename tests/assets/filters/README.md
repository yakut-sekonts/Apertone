# Redistributable photographic QA fixtures

These are small, already-developed sRGB photographs, **not RAW/sensor reference
images**. They test authored Curve/HSL looks on skin/orange fabric, vegetation/
water/sky and dark architectural scenes. Bayer decoding and actual Nikon NEF
acceptance remain independent. No private user photograph is published here.

- `portrait.png`: NASA portrait of astronaut Eileen Collins, distributed in
  [scikit-image v0.25.2](https://github.com/scikit-image/scikit-image/blob/v0.25.2/skimage/data/astronaut.png).
  [Upstream attribution/public-domain status](https://scikit-image.org/docs/stable/api/skimage.data.html#skimage.data.astronaut)
  and [NASA media guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media/).
  Editorial software QA only; NASA and the person depicted do not endorse Luma.
- `landscape.png`: Mshuang2, *River landscape with rocks and trees*, 2020,
  [author's source/license declaration](https://commons.wikimedia.org/wiki/File:River_landscape_with_rocks_and_trees.jpg),
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- `night.png`: Syd Storm, *Big Ben at night, 30 April 2017*,
  [author's source/license declaration](https://commons.wikimedia.org/wiki/File:Big_Ben_at_night,_30_April_2017.jpg),
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

License/source pages were checked on 2026-10-14. `manifest.json` records authors,
URLs and original/derivative SHA256. These licenses/public-domain status apply to
the image assets independently of the repository's Apache-2.0 source license.
Only the PNG derivatives are checked in. ImageIO applied EXIF orientation, resized
to at most 512px and wrote PNG without copying source metadata. `prepare.swift`
records the conversion: download the manifest URLs into `.tools/filter-fixtures`
with names `portrait.png`, `landscape-original.jpg`, `night-original.jpg`, then run
`xcrun swift tests/assets/filters/prepare.swift` from the repository root. Regenerated
bytes can differ across ImageIO versions; review before updating the manifest.

The filter quality test decodes sRGB to linear RGB and uses Luma's shared renderer.
It verifies neutral pixel identity, finite signed/HDR values, monotone curves,
idempotence, excluded groups/flags, JSON roundtrip, deterministic render output,
clipping increase (at most two percentage points) and reviewed RGB/difference/
clipping plus 4×4 spatial RGB metrics. Tolerance is 0.4 of one 8-bit code value
(or percentage point for clipping), allowing platform rounding without a
bit-for-bit ImageIO dependency. The test writes contact sheet/metrics under the
build's `qa/filters` directory; it never updates reference files.

`reference.json` is a reviewed baseline for these exact small fixtures. It detects
regressions; it does not establish a universal aesthetic or a camera colorimetric
reference. Filter values are original Luma presets, not emulations of commercial
film stocks, LUTs or camera profiles.
