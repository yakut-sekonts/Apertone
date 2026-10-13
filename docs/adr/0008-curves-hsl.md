# ADR 0008: Curves and HSL as part of the edit document

Point curves use 2–32 normalized points per RGB/channel curve and a 4097-entry
PCHIP lookup table with shape-preserving slopes. Endpoints have fixed x=0/1;
output values may be creative/non-monotone. Endpoint linear extensions preserve
negative/HDR input without introducing clipping. Parametric curves use four
smooth regional exposure weights. HSL uses eight circular, overlapping hue bands
with bounded Hue/Saturation/Luminance and neutral protection.

Advanced color works in perceptually encoded sRGB, after basic adjustments, then
returns to linear output. This is a project-defined pipeline, not an Adobe engine
or a calibrated camera profile. Neutral/disabled groups skip the new operations,
so preceding Develop renders remain identical. Preview and all export formats
share the implementation. CPU processing remains the reference backend.

Edit schema 3 and catalog schema 4 store the full edit atomically. Point/HSL tables
use asset foreign keys and explicit positions/band indices. The reader rejects
incomplete persisted advanced documents. Migration keeps old edits, enabled states,
Library metadata and selection. Undo remains session-local, one photo gesture per
operation. Presets, Detail, Crop and Optics are separate later substeps of 0.4.

PCHIP math follows the [documented weighted harmonic slope method](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html)
without importing SciPy or copying its implementation.
