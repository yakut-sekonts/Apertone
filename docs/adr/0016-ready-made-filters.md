# 0016 — Authored photographic filters as ordinary presets

Status: accepted for phase 0.4, Issue #26.

Ship eight immutable authored looks as declarative values in the existing Curve
and HSL groups. No extra rendering pass, opaque LUT or edit schema is introduced.
These looks intentionally replace Curve/HSL; reapplying is idempotent. Exposure,
White Balance, Light/Color, Detail, Crop, Optics, Geometry and their enable states
remain each photo's own values. Users can adjust every applied parameter normally.
Natural resets just Curve/HSL. No approximate monochrome or film-emulation claim.

Use stable reserved `builtin:` IDs, independent of user storage. The controller
exposes built-ins separately, resolves both for hover/application/batch snapshots,
and refuses editing/deleting built-in IDs. JSON export uses the ordinary preset
format; importing produces an editable user copy with a fresh ID. Merely opening
or applying a built-in never writes the user preset collection.

QML displays an accessible list before user presets. Hover uses existing transient
preview with revision guards; click uses existing atomic subset application and
one Undo. Batch application uses the same Curve/HSL subset for every target.

Representative portrait, vegetation/sky/water and low-light architectural fixtures
are redistributed with verified public-domain/CC0 provenance, authors, source and
license links, conversion notes and SHA256 checksums. Tests load these sRGB photos
into linear RGB for look regression only, independently from RAW/sensor acceptance.
Review contact sheets and numerical invariants; do not claim universal photographic
quality or broaden phase 0.4 into local masks or new effects.
