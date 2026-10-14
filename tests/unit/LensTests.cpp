#include "core/lens/LensCorrection.h"
#include "core/render/CpuRenderer.h"
#include <algorithm>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid optics were accepted.");
}
bool near(double a, double b, double tolerance = 1e-9) {
    return std::abs(a - b) <= tolerance;
}
luma::PhotoMetadata nikon() {
    luma::PhotoMetadata metadata;
    metadata.camera = "NIKON CORPORATION NIKON D7500";
    metadata.lens = "AF-S DX Nikkor 18-140mm f/3.5-5.6G ED VR";
    metadata.focalLength = 18;
    metadata.aperture = 5.6;
    return metadata;
}
void identification() {
    require(luma::lensProfiles().size() > 1000 && luma::lensCameras().size() > 500,
            "Pinned calibration database is incomplete.");
    auto metadata = nikon();
    const auto resolved = luma::resolveLens(metadata, {});
    require(resolved.camera && resolved.profile && resolved.distortion && resolved.tca &&
                resolved.vignetting,
            "Known Nikon aliases/calibrations did not resolve.");
    require(near(resolved.camera->cropFactor, 1.534) &&
                resolved.profile->name.find("18-140") != std::string_view::npos,
            "Wrong camera or lens selected.");
    require(resolved.distortion->model == "ptlens" && near(resolved.distortion->terms[0], 0.02),
            "Database calibration was altered.");
    auto settings = luma::LensSettings{};
    settings.profile = resolved.profile->id;
    metadata.lens = "Nikon lens ID 123";
    require(!luma::resolveLens(metadata, {}).profile &&
                luma::resolveLens(metadata, settings).profile == resolved.profile,
            "Ambiguous lens was guessed or manual selection failed.");
    metadata.camera = "Unknown Camera";
    require(!luma::resolveLens(metadata, settings).profile, "Unknown sensor was guessed.");
    metadata = nikon();
    metadata.focalLength = 16;
    const auto outside = luma::resolveLens(metadata, {});
    require(outside.profile && !outside.distortion && !outside.tca && !outside.vignetting,
            "Calibration was extrapolated beyond focal range.");
    metadata.focalLength = 26.5;
    const auto middle = luma::resolveLens(metadata, {});
    require(middle.distortion && near(middle.distortion->terms[0], 0.015),
            "Focal interpolation is wrong.");
    metadata = nikon();
    metadata.aperture.reset();
    const auto noAperture = luma::resolveLens(metadata, {});
    require(noAperture.distortion && noAperture.tca && !noAperture.vignetting,
            "Missing aperture contaminated geometry.");
    settings.profile = "0123456789abcdef0123456789abcdef";
    require(!luma::resolveLens(metadata, settings).profile,
            "Missing profile was silently replaced.");
    metadata.camera = "Canon EOS 7D";
    metadata.lens = "Canon EF 24-105mm f/4L IS USM";
    metadata.focalLength = 24;
    metadata.aperture = 4;
    const auto ambiguous = luma::resolveLens(metadata, {});
    require(ambiguous.camera && !ambiguous.profile &&
                ambiguous.status.find("Ambiguous") != std::string::npos,
            "Duplicate compatible calibration profiles were guessed.");
    const auto chosen = std::find_if(luma::lensProfiles().begin(), luma::lensProfiles().end(),
                                     [&](const auto &profile) {
                                         return profile.name == metadata.lens &&
                                                luma::compatibleLens(*ambiguous.camera, profile);
                                     });
    require(chosen != luma::lensProfiles().end(), "Ambiguous manual profile fixture missing.");
    settings.profile = chosen->id;
    require(luma::resolveLens(metadata, settings).profile == &*chosen,
            "Manual selection did not resolve an ambiguous lens.");
}
void equations() {
    const luma::LensCamera camera{"Fixture", {}, "Fixture", 1};
    const luma::LensProfile profile{"", "Fixture", {}, {}, 1, 1.5, 0, 0, {}, {}, {}};
    luma::ResolvedLens resolved;
    resolved.camera = &camera;
    resolved.profile = &profile;
    const double norm = 2 / std::hypot(120, 80) * std::hypot(1.5, 1);
    for (const auto model : {"poly3", "poly5", "ptlens"}) {
        resolved.distortion = luma::LensCalibration{model, 35, 0, 0, {0.015, -0.003, 0.006}};
        const luma::LensMapping mapping(121, 81, resolved, {});
        const double x = 81, y = 49;
        const double nx = (x - 60) * norm / mapping.zoom(), ny = (y - 40) * norm / mapping.zoom();
        const double r = std::hypot(nx, ny);
        const double factor = std::string_view(model) == "poly3" ? 0.985 + 0.015 * r * r
                              : std::string_view(model) == "poly5"
                                  ? 1 + 0.015 * r * r - 0.003 * std::pow(r, 4)
                                  : 0.982 + 0.006 * r - 0.003 * r * r + 0.015 * r * r * r;
        const auto actual = mapping.source(x, y, 1);
        require(near(actual[0], 60 + nx * factor / norm) &&
                    near(actual[1], 40 + ny * factor / norm),
                "Distortion equation/direction is wrong.");
        const luma::LensMapping larger(241, 161, resolved, {});
        const auto large = larger.source(x * 2, y * 2, 1);
        require(near(large[0], actual[0] * 2) && near(large[1], actual[1] * 2),
                "Preview/full-resolution normalization diverged.");
        for (int py = 0; py < 81; ++py)
            for (int px = 0; px < 121; ++px)
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const auto point = mapping.source(px, py, channel);
                    require(point[0] >= -1e-9 && point[0] <= 120 + 1e-9 && point[1] >= -1e-9 &&
                                point[1] <= 80 + 1e-9,
                            "Safe optical canvas has empty edges.");
                }
    }
    resolved.distortion.reset();
    resolved.tca = luma::LensCalibration{"linear", 35, 0, 0, {1.01, 0.99}};
    const luma::LensMapping linear(121, 81, resolved, {});
    const auto green = linear.source(80, 50, 1), red = linear.source(80, 50, 0),
               blue = linear.source(80, 50, 2);
    require(near(red[0] - 60, (green[0] - 60) * 1.01) && near(blue[0] - 60, (green[0] - 60) * 0.99),
            "Lateral CA does not map each sensor channel separately.");
    resolved.tca =
        luma::LensCalibration{"poly3", 35, 0, 0, {0.001, -0.002, 1.001, -0.001, 0.002, 0.999}};
    const luma::LensMapping polynomial(121, 81, resolved, {});
    const auto g = polynomial.source(80, 50, 1), r = polynomial.source(80, 50, 0);
    const double radius = std::hypot(g[0] - 60, g[1] - 40) * norm;
    require(near(r[0] - 60, (g[0] - 60) * (0.001 * radius * radius - 0.002 * radius + 1.001)),
            "Polynomial CA equation is wrong.");
    resolved.tca.reset();
    resolved.vignetting = luma::LensCalibration{"pa", 35, 5.6, 1000, {-0.2, 0, 0}};
    const luma::LensMapping vignette(121, 81, resolved, {});
    require(!vignette.geometryActive() && vignette.gain(60, 40) == 1 &&
                near(vignette.gain(0, 0), 1.25),
            "Vignette gain is not reciprocal or diagonal normalized.");
    const luma::LensCamera cropped{"Cropped", {}, "Fixture", 2};
    resolved.camera = &cropped;
    const luma::LensMapping cropSensor(121, 81, resolved, {});
    require(near(cropSensor.gain(0, 0), 1 / 0.95), "Calibration crop factors were ignored.");
    resolved.camera = &camera;
    resolved.distortion = luma::LensCalibration{"poly3", 35, 0, 0, {-2}};
    rejects([&] { luma::LensMapping unsafe(121, 81, resolved, {}); });
}
void pixels() {
    luma::FloatImage image(121, 81);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
            image.rgb[offset] = static_cast<float>(x * 0.01 - 0.7);
            image.rgb[offset + 1] = static_cast<float>(y * 0.03 + 0.2);
            image.rgb[offset + 2] = -2;
        }
    auto metadata = nikon();
    luma::EditDocument edits;
    const luma::LensCorrectionPass disabled(image, edits, metadata);
    require(!disabled.active() && &disabled.image() == &image,
            "Disabled optics copied or altered pixels.");
    const auto baseline = luma::renderCPU(image, edits).rgba;
    edits.setEnabled(luma::Correction::lens, true);
    edits.lens.distortion = edits.lens.vignetting = 0;
    edits.lens.chromaticAberration = false;
    const luma::LensCorrectionPass zero(image, edits, metadata);
    require(
        !zero.active() && &zero.image() == &image &&
            luma::renderCPU(image, edits, 0, {}, {}, {}, luma::DetailDiagnostic::none, 1, metadata)
                    .rgba == baseline,
        "Zero optics changed pixels or allocated a buffer.");
    edits.lens.distortion = 100;
    const auto resolved = luma::resolveLens(metadata, edits.lens);
    const luma::LensMapping mapping(image.width, image.height, resolved, edits.lens);
    const luma::LensCorrectionPass geometry(image, edits, metadata);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const auto point = mapping.source(x, y, 0);
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
            require(near(geometry.image().rgb[offset], point[0] * 0.01 - 0.7, 1e-7) &&
                        near(geometry.image().rgb[offset + 1], point[1] * 0.03 + 0.2, 3e-7) &&
                        geometry.image().rgb[offset + 2] == -2,
                    "Bilinear optics clipped signed HDR or did not interpolate an affine ramp.");
        }
    edits.lens.distortion = 0;
    edits.lens.vignetting = 100;
    const luma::LensCorrectionPass vignette(image, edits, metadata);
    require(vignette.image().rgb[2] < -2 && vignette.image().rgb.back() < -2,
            "Optical gain clipped negative values.");
    metadata.camera = "Unknown";
    require(!luma::LensCorrectionPass(image, edits, metadata).active(),
            "Unknown camera modified pixels.");
    luma::CancellationSource stop;
    stop.cancel();
    rejects([&] { luma::LensCorrectionPass cancelled(image, edits, metadata, stop.token()); });
    for (const auto value : {-1.0, 201.0, std::numeric_limits<double>::infinity()})
        rejects([&] { edits.setParameter("lens.distortion", value); });
    rejects([&] { edits.setParameter("lens.chromaticAberration", 0.5); });
    edits.lens.profile = "not-an-id";
    rejects([&] { edits.validate(); });
}
} // namespace
int main() {
    try {
        identification();
        equations();
        pixels();
        std::cout << "Lens identification, equations, normalization, safe bounds, signed HDR and "
                     "neutral pixels passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
