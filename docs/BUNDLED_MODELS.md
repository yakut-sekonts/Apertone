# Bundled offline model delivery

Decision: the user requested installer-bundled models on 2026-10-21. This is a
source implementation under final qualification. Both models compiled on
2026-10-21 with Xcode 27.1, but **are not qualified**: the Denoise numerical
comparison currently fails. No installer was published. Stable remains 0.5.1.

## Runtime

The app resolves `Contents/Resources/Models/LumaDenoise.mlmodelc` and
`LumaSky.mlmodelc` relative to its own executable. The first launch needs neither
a download nor manual model configuration. Models and photographs stay local.
Settings exposes a custom compiled model override and a return to the included
model, translated into all eight languages. Only custom paths are persisted;
moving the app or installing a Sparkle update resolves bundled paths again.

Missing resources disable the corresponding feature. Present but invalid models
fail explicitly on the worker; no fallback download or fabricated result is used.
Prediction falls back from available compute units to CPU on supported failures.

## Sources and adaptation

`resources/models/sources.json` pins both upstream revisions, source bytes,
license hashes and model contracts. `resources/models/NOTICE.md` documents
upstream attribution and Luma's changes. Both notices and full upstream licenses
travel with the compiled artifacts.

- NAFNet SIDD width32: MIT with retained BasicSR notices, 512×512 Float32 tensors
  and a 64-pixel halo. The sRGB-trained model is wrapped by linear↔sRGB conversion.
  Any RGB pixel outside [0,1] passes through unchanged; no RAW-trained quality
  claim is made. Adaptation v2 preserves spatially uniform RGB tiles exactly and
  uses checkpoint-equivalent spatial slicing/1×1 convolution to avoid the
  observed native stride-2 convolution error. Model ID is recorded with denoised
  DNG provenance; original weights are unchanged.
- U²-Net Sky: MIT, the author's ONNX model from the pinned attributed mirror,
  320×320 sRGB input with ImageNet normalization and the first prediction output.
  No per-image min/max stretching is applied to the probability map.

## Final build preparation only

On Apple Silicon with Xcode and Python 3.12, run:

```sh
bash scripts/build-bundled-models.sh build/models
```

The recipe uses pinned direct conversion dependencies in a separate build-only
virtual environment. All resolved package versions, Python and Xcode versions
are recorded in the generated manifest. Python packages do not ship in the app.
PyTorch 2.10 uses `weights_only=True` for the hash-pinned checkpoint; arbitrary
checkpoint objects and upstream training code are not loaded. The ONNX adapter
refuses external initializer files and nonstandard operator domains.

The recipe downloads exact-sized, SHA-256-pinned source weights into ignored
`.tools/model-sources`, converts Float32 ML Programs for macOS 14+, invokes the
Core ML compiler and atomically installs a complete artifact directory. Changed
weights/recipe/license files require rebuilding models. Existing outputs are
never silently overwritten; use a fresh output directory for a new recipe.

CMake's default build installs the verified artifact directory into `Luma.app`.
Configuring version 1.0 or later without model artifacts fails. `package.sh`
requires both models and copies them before signing; `BUILD_INFO.json` records
their identities and manifest hash. A custom artifact root can be selected with
the CMake cache setting and packaging environment variable
`LUMA_BUNDLED_MODELS_DIR`; both must point to the same recipe's output.

The release workflow builds models before configuring the app. ZIP publication
checks the pinned recipe, complete file inventory and hashes without extraction.
Models are signed as application resources and included in both the DMG's app
and Sparkle ZIP; there is no independently downloaded model/update channel.

## Qualification

The conversion recipe and Core ML compilation have run. The synthetic CPU
comparison passes for Sky and fails for Denoise; see
[the qualification record](FINAL_1_0_READINESS.md#completed-baseline-and-current-blockers).
In particular, coremltools 9.0 documents
testing with an earlier PyTorch version than this recipe's patched 2.10 loader;
conversion accuracy must be established during final preparation.

Run the numerical comparison with:

```sh
.tools/model-venv/bin/python scripts/check-model-conversion.py build/models --report build/model-numerics.json
```

This uses the adapted source networks and a standalone native Core ML CPU probe.
It checks eight Denoise inputs (including uniform colors, dark noise and an
isolated impulse) and three Sky inputs, finite Float32 tensor contracts, Sky
bounds, and exact Denoise passthrough for uniform tiles/out-of-range pixels.
Use `--compute-units all` or `--compute-units cpu-and-gpu` to check another Core
ML execution policy. The policy does not establish actual device placement.
The latest local v2 result is 9/11 PASS on CPU and 10/11 with ALL; dark nonuniform
inputs still fail. It is an additional CI/release gate, not photographic or tiling
qualification. Native
prediction failures and signals fail the report; reports are never overwritten.
CI continues independent application checks after a failed comparison, then
requires its successful outcome in a final mandatory step. The release workflow
still stops immediately; neither path permits a failed model gate to pass.

Source quality has a separate mandatory gate:

```sh
.tools/model-venv/bin/python scripts/check-denoise-quality.py build/models --report build/denoise-quality.json
```

It compares predictions with analytical clean ramps/colors and a known impulse
corruption, independently of Core ML. For noisy inputs, squared error must
decrease and peak error must not exceed the input's peak error; noiseless uniform
colors must remain exact. Both the full tensor and the runtime's 64-pixel crop
must pass. These are fixed criteria, not limits fitted to the current model.
Identity, bright-output and detail-destroying predictors are explicitly rejected
by oracle tests. The current v2 source result is **5/7 PASS, overall FAIL**: both
nonuniform dark cases fail, including the interior crop. The same failure occurs
before Core ML and cannot be waived by source/native numerical agreement.
CI preserves both reports and requires both outcomes; release packaging stops
on either failure. Photographic and runtime quality still need separate evidence.

Require representative RAW/ordinary-image Sky scenes, denoise texture/color/noise
comparisons, HDR/negative-sample preservation, tiled edges, no-sky false positives,
orientation/crop alignment, CPU fallback, cancellation and 8/16 GB resource tests.
Verify first offline launch, app moves, custom override/reset, signed Applications
installation and Sparkle upgrade with both models. A generated model manifest
records `qualification: NOT_RUN`; model quality is established by separate final
release evidence, not conversion success. See [final readiness](FINAL_1_0_READINESS.md).
