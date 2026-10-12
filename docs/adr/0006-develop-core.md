# ADR 0006: Develop Core and export snapshots

Stage 0.2 keeps normalized demosaiced camera RGB separate from As Shot gains and
the camera matrix. WB is evaluated before conversion to scene-linear sRGB.
Preview and full-resolution export share the CPU Develop implementation.
Temperature/Tint use a Planckian white-point approximation; no production camera
profile or Lightroom rendering equivalence is claimed.

Edit document/schema 2 contains typed numeric parameters and independent enable
flags. A transactional migration preserves M0 exposure, revisions and timestamps.
Unknown future catalog/edit versions are refused. The latest document is durable;
bounded gesture-based undo/redo is in-memory per active-photo session. Persistent
history, versions, Auto WB and eyedropper are later work.

Exports decode and verify the source again, then render an immutable edit
snapshot at original resolution. ImageIO writes JPEG, PNG or 16-bit TIFF with an
embedded sRGB color space, without source metadata. Monitor ICC is never used in
export. A sibling temporary file is installed atomically, with explicit overwrite
policy and protection against replacing the original. Advanced output spaces,
resize, sharpening, batch queue, metadata selection and DNG remain later work.

WB equations are checked against the [Colour project reference implementation](https://colour.readthedocs.io/en/develop/_modules/colour/temperature/kang2002.html). Tint is a project-defined xy offset, not a camera-calibrated Duv value.
