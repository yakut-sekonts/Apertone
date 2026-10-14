# Ready-made filters — 0.4.8 development

[Issue #26](https://github.com/yakut-sekonts/Luma/issues/26) adds Natural, Soft
Portrait, Landscape, Warm Film, Cool Cinema, Matte, Vintage and Crisp. Each is an
original declarative Curve/HSL preset; there is no extra rendering pass or opaque
LUT. Applied values are visible and editable. Looks replace Curve/HSL, so repeated
application is idempotent; Natural resets only those groups. Exposure, WB, Detail,
Crop, Optics and Geometry retain their per-photo values and enable states.

Separate tabs keep the built-in and user lists within the available panel height.
The built-in list is separate from user storage and uses stable reserved IDs.
Renaming/deleting built-ins is refused. Export JSON/import creates an editable
user copy. Hover uses the existing transient preview, click applies one Undo,
and batch snapshots use the same subset behavior. Loading or applying a built-in
does not write the user preset collection. No catalog/edit/preset schema changes.
See ADR 0016.

Three small licensed photographs cover skin/orange fabric, vegetation/sky/water
and dark architecture. Source and license declarations, author names, original/
derivative SHA256, metadata-stripping conversion and limitations are committed.
Photographic QA uses sRGB→linear input with the shared renderer, reviewed contact
sheets and RGB/difference/clipping plus 4×4 spatial color reference metrics.
This supplements generated RAW/QML and private Nikon acceptance; developed
photographs are not a sensor colorimetric reference or proof of universal taste.

Local validation passed: all 26 CTests (including private Nikon), clang-format
and clang-tidy; targeted checks also passed after the final tab/batch UI changes.
Coverage includes immutable/unique IDs, JSON roundtrip, HDR finite
values, monotone curves, neutral exact pixels, deterministic/idempotent looks,
excluded fields/flags, actual QML hover/click/leave/Undo/Redo, autosave/restart,
export equality and atomic batch Undo/Redo. Check clang-format/clang-tidy,
exact-head macOS CI and standalone bundle before integration.

Edited thumbnails and advanced export/queue remain phase 0.4. No phase 0.5 work
is included. This is a development version, not a signed/notarized public release.
