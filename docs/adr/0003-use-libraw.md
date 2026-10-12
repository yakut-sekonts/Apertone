# 0003 — LibRaw sensor decoder
Status: Accepted for M0.

Context: Nikon D7500 NEF is the first required acceptance asset. Develop must
process sensor data rather than embedded JPEGs.

Decision: LibRaw 0.21.5 `open_file` and `unpack`, dynamically linked under LGPL-2.1.
Normalization, WB, bilinear demosaic and Float32 rendering belong to Luma.

Consequences: M0 handles integer Bayer only and must reject unsupported layouts.
There is no claim of a production demosaic algorithm yet. Optional GPL packs are
not enabled. Genuine NEF testing uses an external private directory.
