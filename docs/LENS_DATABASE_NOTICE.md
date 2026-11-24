# Lens calibration database attribution

Source: Lensfun contributors / Tom Niemann (original PTLens database).
https://github.com/lensfun/lensfun/tree/v0.3.4/data/db

License: Creative Commons Attribution-ShareAlike 3.0 Unported.
https://creativecommons.org/licenses/by-sa/3.0/
The full license accompanies this notice in the bundle.

The pinned v0.3.4 XML files are unchanged. One additional M42 profile for the
LZOS Industar-61 L/Z MC 50mm f/2.8 comes from Lensfun revision
[`37238fce`](https://github.com/lensfun/lensfun/blob/37238fcef563196fb798f6151cb9023968b26c40/data/db/misc.xml).
`resources/lens/industar-61.xml` preserves its measured coefficients and adds
Russian names. Both sources use the license above.

Apertone converts these data into sorted arrays in `LensDatabase.h`, with
stable profile IDs, barrel markings and nominal focal lengths for identified
manual primes. It supports poly3/poly5/PTLens distortion, linear/poly3 TCA
and PA vignetting. For the Zenit Zenitar MC 16mm f/2.8, the measured poly3
distortion and lateral CA operate in the native fisheye projection.
This generated representation is an adaptation of the database and retains
CC BY-SA 3.0. The data license is separate from Apertone's original source license.
No endorsement by the database contributors is implied.

Reproduce the representation with `scripts/compile-lens-database.py`, the
included XML supplement and the SHA256-pinned archive in `cmake/Dependencies.cmake`. Apertone does not incorporate
Lensfun library implementation code.
