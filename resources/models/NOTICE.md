# Luma bundled offline models

The installer and Sparkle update archive carry the same compiled Core ML models
under `Luma.app/Contents/Resources/Models`. Runtime processing stays on the Mac;
Luma does not download weights or send photographs to a model service.
Python, PyTorch and conversion tools are build dependencies and are not shipped.

`sources.json` pins the source revision, exact weight bytes and upstream licenses.
The generated `manifest.json` records the conversion recipe, toolchain and every
artifact checksum. It records provenance, not a successful quality qualification.

## Denoise: NAFNet SIDD width32

Copyright (c) 2022 megvii-model. Upstream NAFNet is MIT-licensed; its full license
and inherited BasicSR Apache-2.0 notices are in `licenses/NAFNet-LICENSE.txt`.
Source: https://github.com/megvii-research/NAFNet/tree/2b4af71ebe098a92a75910c233a3965a3e93ede4
The checkpoint is the author's SIDD width32 model linked from that revision's
`readme.md`; the download URL, size and SHA-256 are recorded in `sources.json`.

Luma modifications: an inference-only architecture preserving checkpoint keys,
ordinary tensor LayerNorm, fixed 512×512 input, Float32 Core ML conversion,
linear-sRGB to sRGB input conversion and the inverse output conversion. The
native adapter uses reflected padding and a 64-pixel halo. RGB pixels with any
input channel outside [0,1] pass through unchanged to preserve negative samples
and HDR highlights; Amount blends the prediction with the original.

The training domain is display-referred SIDD, not RAW sensor data. This is not a
claim of camera-independent RAW denoise accuracy. Fine detail, color, highlights,
tile boundaries and noise behavior require photographic qualification. No
training dataset or upstream training code is included in the app.

## Sky: U²-Net Sky

Copyright (c) 2021 xiongzhu666. MIT license is reproduced in
`licenses/SkySeg-LICENSE.txt`.
Source: https://github.com/xiongzhu666/Sky-Segmentation-and-Post-processing/tree/1f7811b32b64ddc957269defff84bc87a3f0b74f
The ONNX weights are distributed by the attributed MIT model mirror:
https://huggingface.co/JianyuanWang/skyseg/tree/3ba8c6df1d9ba9ff26f637c7ba9568ac11a9aa7f

Luma modifications: use the first prediction output, wrap the author's ImageNet
RGB mean/std normalization, fixed 320×320 Float32 input, ONNX-to-Core-ML
conversion and finite probability coverage in the native mask pipeline. Input
is top-left sRGB with bilinear stretch resizing. Output is clamped to [0,1];
unlike the author's demonstration, Luma does not stretch each image's prediction
minimum/maximum to 0/1, which could amplify a weak no-sky response.

Sky boundaries, reflections, indoor scenes, thin foreground objects and images
without sky require qualification. Model confidence is not a guarantee of a
correct selection; the resulting mask remains editable.
