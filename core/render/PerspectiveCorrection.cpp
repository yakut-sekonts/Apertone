#include "core/render/PerspectiveCorrection.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <optional>

namespace luma {
namespace {
using Vector = std::array<double, 3>;
Vector cross(const Vector &a, const Vector &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
std::optional<Vector> vanishing(const std::vector<PerspectiveGuide> &guides, GuideAxis axis) {
    std::vector<Vector> lines;
    for (const auto &guide : guides)
        if (guide.axis == axis) {
            const auto a = guide.first, b = guide.second;
            auto line = cross({2 * a[0] - 1, 2 * a[1] - 1, 1}, {2 * b[0] - 1, 2 * b[1] - 1, 1});
            const double norm = std::hypot(line[0], line[1]);
            for (auto &value : line)
                value /= norm;
            lines.push_back(line);
        }
    if (lines.empty())
        return {};
    if (lines.size() != 2)
        throw std::runtime_error(
            "Guided correction needs exactly two lines per selected direction.");
    auto point = cross(lines[0], lines[1]);
    const double norm = std::hypot(point[0], point[1]);
    if (norm < 0.025)
        throw std::runtime_error("Guides overlap or have no stable vanishing direction.");
    for (auto &value : point)
        value /= norm;
    const std::size_t component = axis == GuideAxis::vertical ? 1 : 0;
    if (std::abs(point[component]) < 0.2 ||
        (std::abs(point[2]) > 1e-9 && std::abs(point[component] / point[2]) < 1.2))
        throw std::runtime_error(
            "Guides converge inside the image or do not support their selected direction.");
    if (point[component] < 0)
        for (auto &value : point)
            value = -value;
    return point;
}
struct Edge {
    double x, y, gx, gy, magnitude;
};
struct Candidate {
    PerspectiveGuide guide;
    double strength;
};
std::vector<Candidate> candidates(const std::vector<Edge> &edges, int width, int height,
                                  GuideAxis axis, const CancellationToken &stop) {
    const int radius = static_cast<int>(std::ceil(std::hypot(width, height)));
    const int bins = radius * 2 + 1, angles = 71;
    // Keep gradient polarity separate: unrelated alternating texture must not
    // accumulate into a fictitious long straight edge.
    std::vector<int> votes(static_cast<std::size_t>(bins) * angles * 2);
    std::array<std::array<std::size_t, angles>, 2> eligible{};
    std::array<double, angles> cosine{}, sine{};
    for (int a = 0; a < angles; ++a) {
        const double radians =
            (a - 35 + (axis == GuideAxis::horizontal ? 90 : 0)) * std::numbers::pi / 180;
        cosine[static_cast<std::size_t>(a)] = std::cos(radians);
        sine[static_cast<std::size_t>(a)] = std::sin(radians);
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i % 256 == 0)
            checkCancellation(stop);
        const auto &edge = edges[i];
        for (int a = 0; a < angles; ++a) {
            const double c = cosine[static_cast<std::size_t>(a)],
                         s = sine[static_cast<std::size_t>(a)];
            const double gradient = edge.gx * c + edge.gy * s;
            if (std::abs(gradient) < edge.magnitude * 0.9)
                continue;
            const int rho = static_cast<int>(std::lround(edge.x * c + edge.y * s)) + radius;
            if (rho >= 0 && rho < bins) {
                const std::size_t side = gradient > 0 ? 1 : 0;
                ++votes[(side * angles + a) * bins + rho];
                ++eligible[side][static_cast<std::size_t>(a)];
            }
        }
    }
    // Dense texture can collect many unrelated points in a narrow strip. A
    // peak must exceed the background expected from the direction's gradient
    // density, with a conservative allowance for the smoothing correlation.
    for (std::size_t a = 0; a < angles; ++a) {
        const double chord = std::min(width / std::max(1e-9, std::abs(sine[a])),
                                      height / std::max(1e-9, std::abs(cosine[a])));
        for (std::size_t side = 0; side < 2; ++side) {
            const double background =
                chord * static_cast<double>(eligible[side][a]) / (width * height);
            const double floor = background + 8 * std::sqrt(4 * background + 1);
            for (int rho = 0; rho < bins; ++rho) {
                auto &value = votes[(side * angles + a) * bins + rho];
                if (value < floor)
                    value = 0;
            }
        }
    }
    std::vector<Candidate> result;
    const int minimumVotes = std::max(18, (axis == GuideAxis::vertical ? height : width) / 5);
    for (int attempt = 0; attempt < 24 && result.size() < 6; ++attempt) {
        checkCancellation(stop);
        const auto maximum = std::max_element(votes.begin(), votes.end());
        if (maximum == votes.end() || *maximum < minimumVotes)
            break;
        const auto position = static_cast<std::size_t>(maximum - votes.begin());
        const int angle = static_cast<int>((position / bins) % angles),
                  rho = static_cast<int>(position % bins);
        const double polarity = position / (static_cast<std::size_t>(bins) * angles) == 0 ? -1 : 1;
        const double c = cosine[static_cast<std::size_t>(angle)],
                     s = sine[static_cast<std::size_t>(angle)];
        for (int a = std::max(0, angle - 4); a <= std::min(angles - 1, angle + 4); ++a)
            for (int r = std::max(0, rho - 8); r <= std::min(bins - 1, rho + 8); ++r)
                for (int side = 0; side < 2; ++side)
                    votes[(static_cast<std::size_t>(side) * angles + a) * bins + r] = 0;
        std::vector<const Edge *> inliers;
        double mx = 0, my = 0;
        for (const auto &edge : edges)
            if (std::abs(edge.x * c + edge.y * s - (rho - radius)) < 1.8 &&
                (edge.gx * c + edge.gy * s) * polarity > edge.magnitude * 0.86) {
                inliers.push_back(&edge);
                mx += edge.x;
                my += edge.y;
            }
        if (inliers.size() < static_cast<std::size_t>(minimumVotes))
            continue;
        mx /= static_cast<double>(inliers.size());
        my /= static_cast<double>(inliers.size());
        double xx = 0, yy = 0, xy = 0;
        for (const auto *edge : inliers) {
            const double dx = edge->x - mx, dy = edge->y - my;
            xx += dx * dx;
            yy += dy * dy;
            xy += dx * dy;
        }
        const double direction = std::atan2(2 * xy, xx - yy) / 2;
        const double dx = std::cos(direction), dy = std::sin(direction);
        double first = 1e9, last = -1e9;
        for (const auto *edge : inliers) {
            const double t = (edge->x - mx) * dx + (edge->y - my) * dy;
            first = std::min(first, t);
            last = std::max(last, t);
        }
        const double length = last - first;
        if (length < (axis == GuideAxis::vertical ? height : width) * 0.3 ||
            static_cast<double>(inliers.size()) < length * 0.7)
            continue;
        const int intervals = std::max(1, static_cast<int>(std::ceil(length / 4)));
        std::vector<bool> occupied(static_cast<std::size_t>(intervals));
        for (const auto *edge : inliers) {
            const auto t = (edge->x - mx) * dx + (edge->y - my) * dy;
            occupied[static_cast<std::size_t>(
                std::clamp(static_cast<int>((t - first) / 4), 0, intervals - 1))] = true;
        }
        const double coverage =
            static_cast<double>(std::count(occupied.begin(), occupied.end(), true)) / intervals;
        if (coverage < 0.65)
            continue;
        const auto point = [&](double t) {
            return std::array<double, 2>{std::clamp((mx + dx * t) / (width - 1), 0.0, 1.0),
                                         std::clamp((my + dy * t) / (height - 1), 0.0, 1.0)};
        };
        PerspectiveGuide guide{axis, point(first), point(last)};
        guide.validate();
        result.push_back({guide, length * coverage});
    }
    return result;
}
std::vector<PerspectiveGuide> bestPair(const std::vector<Candidate> &lines, GuideAxis axis,
                                       double aspect) {
    std::vector<PerspectiveGuide> result;
    double score = 0;
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            const auto &a = lines[i].guide, &b = lines[j].guide;
            const std::size_t crossAxis = axis == GuideAxis::vertical ? 0 : 1;
            const double separation = std::abs((a.first[crossAxis] + a.second[crossAxis] -
                                                b.first[crossAxis] - b.second[crossAxis]) /
                                               2);
            if (separation < 0.15)
                continue;
            std::vector<PerspectiveGuide> pair{a, b};
            try {
                solvePerspectiveGuides(pair, aspect);
            } catch (const std::exception &) {
                continue;
            }
            const double candidateScore =
                (lines[i].strength + lines[j].strength) * (1 + separation);
            if (candidateScore > score) {
                score = candidateScore;
                result = pair;
            }
        }
    return result;
}
} // namespace
void PerspectiveGuide::validate() const {
    if (axis != GuideAxis::vertical && axis != GuideAxis::horizontal)
        throw std::runtime_error("Unknown guide direction.");
    for (auto point : {first, second})
        for (double value : point)
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::runtime_error("Guide endpoints must be inside the image.");
    if (std::hypot(first[0] - second[0], first[1] - second[1]) < 0.04)
        throw std::runtime_error("Guide line is too short.");
}
TransformSettings solvePerspectiveGuides(const std::vector<PerspectiveGuide> &guides,
                                         double imageAspect) {
    if (!std::isfinite(imageAspect) || imageAspect < 0.01 || imageAspect > 100 ||
        guides.size() < 2 || guides.size() > 4)
        throw std::runtime_error("Invalid perspective guide set or image aspect.");
    for (const auto &guide : guides)
        guide.validate();
    const auto vertical = vanishing(guides, GuideAxis::vertical),
               horizontal = vanishing(guides, GuideAxis::horizontal);
    Homography projective, alignment;
    if (vertical && horizontal) {
        const auto v = vertical.value(), h = horizontal.value();
        const double det = v[0] * h[1] - v[1] * h[0];
        if (std::abs(det) < 0.15)
            throw std::runtime_error("Guide directions are nearly parallel.");
        projective.values[6] = (-v[2] * h[1] + v[1] * h[2]) / det;
        projective.values[7] = (-v[0] * h[2] + v[2] * h[0]) / det;
        alignment = Homography{{h[0], v[0], 0, h[1], v[1], 0, 0, 0, 1}}.inverse();
    } else if (vertical || horizontal) {
        const bool isVertical = vertical.has_value();
        const auto p = isVertical ? vertical.value() : horizontal.value();
        projective.values[isVertical ? 7 : 6] = -p[2] / p[isVertical ? 1 : 0];
        const double angle = isVertical ? std::atan2(p[0] * imageAspect, p[1])
                                        : -std::atan2(p[1], p[0] * imageAspect);
        alignment = Homography{{std::cos(angle), -std::sin(angle) / imageAspect, 0,
                                std::sin(angle) * imageAspect, std::cos(angle), 0, 0, 0, 1}};
    } else
        throw std::runtime_error("No complete guide pair.");
    const auto solved = alignment * projective;
    TransformSettings result;
    result.guided = true;
    for (std::size_t i = 0; i < result.guide.size(); ++i)
        result.guide[i] = solved.values[i] / solved.values[8];
    result.validate();
    return result;
}
std::vector<PerspectiveGuide> detectPerspectiveGuides(const DisplayImage &image,
                                                      const CancellationToken &stop) {
    if (image.width < 32 || image.height < 32 || image.width > 640 || image.height > 640 ||
        image.rgba.size() != pixelCount(image.width, image.height) * 4)
        throw std::runtime_error("Auto perspective requires a bounded 32–640 pixel preview.");
    const int width = image.width, height = image.height;
    std::vector<double> gray(pixelCount(width, height));
    for (std::size_t i = 0; i < gray.size(); ++i)
        gray[i] = image.rgba[i * 4] * 0.2126 + image.rgba[i * 4 + 1] * 0.7152 +
                  image.rgba[i * 4 + 2] * 0.0722;
    // Stabilize edge orientation against Bayer chroma/demosaic stair steps.
    // Long, densely supported lines are still required after smoothing.
    constexpr std::array<double, 5> weights{1.0 / 16, 4.0 / 16, 6.0 / 16, 4.0 / 16, 1.0 / 16};
    std::vector<double> horizontalBlur(gray.size()), smooth(gray.size());
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < width; ++x)
            for (std::size_t kernel = 0; kernel < weights.size(); ++kernel)
                horizontalBlur[static_cast<std::size_t>(y) * width + x] +=
                    gray[static_cast<std::size_t>(y) * width +
                         std::clamp(x + static_cast<int>(kernel) - 2, 0, width - 1)] *
                    weights[kernel];
    }
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < width; ++x)
            for (std::size_t kernel = 0; kernel < weights.size(); ++kernel)
                smooth[static_cast<std::size_t>(y) * width + x] +=
                    horizontalBlur[static_cast<std::size_t>(std::clamp(
                                       y + static_cast<int>(kernel) - 2, 0, height - 1)) *
                                       width +
                                   x] *
                    weights[kernel];
    }
    gray = std::move(smooth);
    std::vector<Edge> all;
    std::vector<double> magnitudes;
    for (int y = 1; y < height - 1; ++y) {
        checkCancellation(stop);
        for (int x = 1; x < width - 1; ++x) {
            const auto at = [&](int px, int py) {
                return gray[static_cast<std::size_t>(py) * width + px];
            };
            const double gx = at(x + 1, y - 1) + 2 * at(x + 1, y) + at(x + 1, y + 1) -
                              at(x - 1, y - 1) - 2 * at(x - 1, y) - at(x - 1, y + 1);
            const double gy = at(x - 1, y + 1) + 2 * at(x, y + 1) + at(x + 1, y + 1) -
                              at(x - 1, y - 1) - 2 * at(x, y - 1) - at(x + 1, y - 1);
            const double magnitude = std::hypot(gx, gy);
            if (magnitude > 1) {
                all.push_back({static_cast<double>(x), static_cast<double>(y), gx, gy, magnitude});
                magnitudes.push_back(magnitude);
            }
        }
    }
    if (magnitudes.empty())
        throw std::runtime_error("Auto: no confident straight lines.");
    const auto quantile =
        magnitudes.begin() + static_cast<std::ptrdiff_t>(magnitudes.size() * 4 / 5);
    std::nth_element(magnitudes.begin(), quantile, magnitudes.end());
    const double threshold = std::max(24.0, *quantile * 0.5);
    std::erase_if(all, [threshold](const auto &edge) { return edge.magnitude < threshold; });
    const double aspect = static_cast<double>(width - 1) / (height - 1);
    auto vertical = bestPair(candidates(all, width, height, GuideAxis::vertical, stop),
                             GuideAxis::vertical, aspect);
    auto horizontal = bestPair(candidates(all, width, height, GuideAxis::horizontal, stop),
                               GuideAxis::horizontal, aspect);
    auto result = vertical;
    result.insert(result.end(), horizontal.begin(), horizontal.end());
    if (result.empty())
        throw std::runtime_error("Auto: no confident pair of long straight lines.");
    solvePerspectiveGuides(result, aspect);
    checkCancellation(stop);
    return result;
}
} // namespace luma
