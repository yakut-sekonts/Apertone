# XMP preset import / 0.4.14

In Develop, open **Presets → Import**, choose a `.xmp` file and then click the
imported preset to apply it. Import adds a new local preset and does not change
the open photo. Hover previews it; apply is one Undo/Redo operation. The filename
is used when the XMP has no name; localized Camera Raw Name/Group uses x-default,
then English, then the first available entry. Upper-case `.XMP` is accepted.

Apertone imports a compatible subset of Lightroom / Camera Raw settings. Its image
processing differs from Adobe's: a preset does not guarantee identical pixels.
The import notice and **XMP compatibility** dialog list skipped properties.

## Recognized settings

- Exposure2012 / Exposure, Contrast2012 / Contrast; Highlights2012, Shadows2012,
  Whites2012, Blacks2012. Modern values take precedence over legacy duplicates.
- Vibrance, Saturation; absolute Temperature/Tint and Custom / As Shot WB mode.
  Auto and other Adobe WB modes are skipped with their dependent values.
- Parametric Shadows, Darks, Lights, Highlights; RGB and Red/Green/Blue point
  curves, modern PV2012 or legacy arrays. Linear curve names can reset RGB.
- Hue, Saturation and Luminance adjustments for all eight HSL bands.
- Sharpening Amount, Radius, Detail, Masking; luminance/color noise reduction,
  their Detail controls, luminance Contrast and color Smoothness.

Only explicitly present supported parameters are applied. For example, an XMP
containing Exposure2012 alone changes Exposure and its enable switch; existing
Contrast, Highlights, WB and other settings remain unchanged. This also applies
to batch presets, preview, JSON export/reimport and preset rename/move. Recapturing
current settings through the edit dialog intentionally creates whole-group data.

## Compatibility and validation

Profiles and embedded looks/LUTs, masks/local adjustments, calibration, geometry,
lens corrections, Texture, Clarity, Dehaze and AI settings are not converted.
An XMP containing only unsupported settings is rejected instead of adding an
empty preset. This feature imports presets; it is not photo-sidecar synchronization
or XMP metadata export. Native export remains Apertone JSON.

Numeric values must fit Apertone's control ranges and are never silently clamped.
Point curves require 2–32 points with integer coordinates in 0–255, strictly
increasing x, and endpoints at x=0 and x=255. Malformed, out-of-range or
unrepresentable supported settings reject the entire import before storage writes.
Unsupported names are shown literally, without executing embedded resources.

The offline XML reader resolves namespace URIs, accepts attribute and element
forms, and limits each file to 1 MiB, 4096 nodes and depth 32. It rejects DTD/entity
declarations, duplicate Camera Raw properties and multiple Camera Raw descriptions.
It never downloads profiles, resolves external resources or modifies source files.

Sparse presets and collections containing them use preset schema 7. Older Apertone
versions refuse that collection rather than dropping parameter selections. Back
up `presets.json` before downgrading. Whole-group presets retain schema 6; earlier
schemas remain readable. No catalog/edit migration is introduced.

## Verification

Project-authored RDF/XML fixtures cover namespaces, localized names, modern and
legacy keys, numeric and curve validation, unsupported nested looks, parser bounds
and atomic failures. Real editor tests cover no-apply import, hover/apply pixel
agreement, Undo/Redo, durable restart, JSON export/reimport, rename and the QML
compatibility dialog. Batch coverage checks each target's unrelated settings and
batch Undo/Redo. Standalone bundle acceptance imports a synthetic XMP, exports,
restarts and compares output while Qt/Sparkle SDK directories are unavailable.

See [ADR 0023](adr/0023-xmp-preset-import.md) and
[Adobe Camera Raw namespace](https://developer.adobe.com/xmp/docs/xmp-namespaces/crs/).
