# Lens calibration database attribution

Source: Lensfun contributors / Tom Niemann (original PTLens database).
https://github.com/lensfun/lensfun/tree/v0.3.4/data/db

License: Creative Commons Attribution-ShareAlike 3.0 Unported.
https://creativecommons.org/licenses/by-sa/3.0/
The full license accompanies this notice in the bundle.

The original v0.3.4 XML files are distributed unchanged. Luma transforms the
calibration data into `LensDatabase.h`: rectilinear profiles only, supported
poly3/poly5/PTLens distortion, linear/poly3 TCA and PA vignetting, sorted arrays,
explicit defaults, normalized search aliases and stable profile IDs.
This generated representation is an adaptation of the database and retains
CC BY-SA 3.0. The data license is separate from Luma's original source license.
No endorsement by the database contributors is implied.

Reproduce the representation with `scripts/compile-lens-database.py` and the
SHA256-pinned archive in `cmake/Dependencies.cmake`. Luma does not incorporate
Lensfun library implementation code.
