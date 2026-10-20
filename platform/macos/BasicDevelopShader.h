#pragma once
namespace luma::macos {
inline constexpr const char *basicDevelopShader = R"metal(
#include <metal_stdlib>
using namespace metal;

kernel void basicDevelop(device const float *input [[buffer(0)]],
                         device float *output [[buffer(1)]],
                         constant float *p [[buffer(2)]],
                         constant uint &count [[buffer(3)]],
                         device atomic_uint &invalid [[buffer(4)]],
                         uint i [[thread_position_in_grid]]) {
    if (i >= count)
        return;
    float3 camera = float3(input[i * 3], input[i * 3 + 1], input[i * 3 + 2]);
    if (!all(isfinite(camera))) {
        atomic_store_explicit(&invalid, 1u, memory_order_relaxed);
        return;
    }
    camera *= float3(p[9], p[10], p[11]);
    float3 rgb;
    rgb.r = (p[0] * camera.r + p[1] * camera.g + p[2] * camera.b) * p[12];
    rgb.g = (p[3] * camera.r + p[4] * camera.g + p[5] * camera.b) * p[12];
    rgb.b = (p[6] * camera.r + p[7] * camera.g + p[8] * camera.b) * p[12];
    const float3 weights(0.2126f, 0.7152f, 0.0722f);
    const float luminance = dot(weights, rgb);
    if (luminance > 1e-8f) {
        const float l = log2(luminance / 0.18f);
        const float shadow = 1.0f / (1.0f + exp2(l * 0.5f));
        const float highlight = 1.0f - shadow;
        const float delta = p[13] * 0.5f * tanh(l * 0.5f) +
                            p[14] * shadow * 2.0f + p[15] * highlight * 2.0f +
                            p[16] * shadow * shadow + p[17] * highlight * highlight;
        rgb *= exp2(delta);
    }
    const float gray = dot(weights, rgb);
    const float minimum = min(rgb.r, min(rgb.g, rgb.b));
    const float maximum = max(rgb.r, max(rgb.g, rgb.b));
    const float chroma = clamp((maximum - minimum) / max(1e-6f, abs(maximum)), 0.0f, 1.0f);
    const float gain = max(0.0f, (1.0f + p[18]) * (1.0f + p[19] * (1.0f - chroma)));
    rgb = gray + (rgb - gray) * gain;
    if (!all(isfinite(rgb))) {
        atomic_store_explicit(&invalid, 1u, memory_order_relaxed);
        return;
    }
    output[i * 3] = rgb.r;
    output[i * 3 + 1] = rgb.g;
    output[i * 3 + 2] = rgb.b;
}
)metal";
} // namespace luma::macos
