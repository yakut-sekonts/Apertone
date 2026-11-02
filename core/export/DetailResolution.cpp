#include "core/export/DetailResolution.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>

namespace luma {
namespace {
double luminance(const LinearOutput &image, int x, int y) {
    const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
    return image.rgba[offset] * .2126 + image.rgba[offset + 1] * .7152 +
           image.rgba[offset + 2] * .0722;
}

double lanczos(double distance) {
    distance = std::abs(distance);
    if (distance < 1e-12)
        return 1;
    if (distance >= 3)
        return 0;
    const double piDistance = std::numbers::pi * distance;
    return 3 * std::sin(piDistance) * std::sin(piDistance / 3) / (piDistance * piDistance);
}

using Filter = std::vector<std::vector<std::pair<int, double>>>;

std::vector<double> gaussianKernel(double sigma) {
    if (!std::isfinite(sigma) || sigma <= 0)
        throw std::runtime_error("Invalid Detail Gaussian radius.");
    const int radius = static_cast<int>(std::ceil(2.5 * sigma));
    std::vector<double> result(static_cast<std::size_t>(radius * 2 + 1));
    double total = 0;
    for (int position = -radius; position <= radius; ++position) {
        const double normalized = static_cast<double>(position) / sigma;
        const double weight = std::exp(-.5 * normalized * normalized);
        const auto index = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(position) + radius);
        result[index] = weight;
        total += weight;
    }
    for (auto &weight : result)
        weight /= total;
    return result;
}

std::vector<float> gaussianLuminance(const LinearOutput &image, double sigma,
                                     CancellationToken stop,
                                     const std::function<void(double)> &progress) {
    const auto kernel = gaussianKernel(sigma);
    const int radius = static_cast<int>(kernel.size() / 2);
    const auto count = pixelCount(image.width, image.height);
    std::vector<float> horizontal(count), blurred(count);
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            double sum = 0, total = 0;
            for (int offset = -radius; offset <= radius; ++offset) {
                const int sampleX = std::clamp(x + offset, 0, image.width - 1);
                const auto sample = (static_cast<std::size_t>(y) * image.width + sampleX) * 4;
                const double coverage = image.rgba[sample + 3];
                const auto kernelIndex =
                    static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + radius);
                const double weight = kernel[kernelIndex] * coverage;
                sum += luminance(image, sampleX, y) * weight;
                total += weight;
            }
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            horizontal[index] =
                static_cast<float>(total > 1e-12 ? sum / total : luminance(image, x, y));
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.45 * static_cast<double>(y + 1) / image.height);
    }
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            double sum = 0, total = 0;
            for (int offset = -radius; offset <= radius; ++offset) {
                const int sampleY = std::clamp(y + offset, 0, image.height - 1);
                const auto sample = (static_cast<std::size_t>(sampleY) * image.width + x) * 4;
                const double coverage = image.rgba[sample + 3];
                const auto kernelIndex =
                    static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + radius);
                const double weight = kernel[kernelIndex] * coverage;
                sum += horizontal[static_cast<std::size_t>(sampleY) * image.width + x] * weight;
                total += weight;
            }
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            blurred[index] =
                static_cast<float>(total > 1e-12 ? sum / total : luminance(image, x, y));
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.45 + .45 * static_cast<double>(y + 1) / image.height);
    }
    return blurred;
}

Filter lanczosWeights(int source) {
    if (source > 10000)
        throw std::runtime_error("2x Detail exceeds the 100 MP output budget.");
    const int target = source * 2;
    Filter result(static_cast<std::size_t>(target));
    for (int index = 0; index < target; ++index) {
        const double center = (index + .5) / 2 - .5;
        auto &weights = result[static_cast<std::size_t>(index)];
        for (int sample = static_cast<int>(std::floor(center)) - 2;
             sample <= static_cast<int>(std::floor(center)) + 3; ++sample) {
            const int bounded = std::clamp(sample, 0, source - 1);
            const double weight = lanczos(center - sample);
            const auto existing =
                std::find_if(weights.begin(), weights.end(),
                             [bounded](const auto &entry) { return entry.first == bounded; });
            if (existing == weights.end())
                weights.emplace_back(bounded, weight);
            else
                existing->second += weight;
        }
        double total = 0;
        for (const auto &[sample, weight] : weights) {
            (void)sample;
            total += weight;
        }
        if (!std::isfinite(total) || std::abs(total) < 1e-12)
            throw std::runtime_error("Invalid 2x Detail resampling kernel.");
        for (auto &[sample, weight] : weights) {
            (void)sample;
            weight /= total;
        }
    }
    return result;
}

std::vector<float> horizontalRow(const LinearOutput &input, int row, const Filter &filter,
                                 CancellationToken stop) {
    std::vector<float> result(filter.size() * 4);
    for (std::size_t x = 0; x < filter.size(); ++x) {
        if ((x & 2047) == 0)
            checkCancellation(stop);
        auto *target = result.data() + x * 4;
        for (const auto &[sample, weight] : filter[x]) {
            const auto source = (static_cast<std::size_t>(row) * input.width + sample) * 4;
            const double alpha = input.rgba[source + 3];
            for (std::size_t channel = 0; channel < 3; ++channel)
                target[channel] +=
                    static_cast<float>(input.rgba[source + channel] * alpha * weight);
            target[3] += static_cast<float>(alpha * weight);
        }
    }
    return result;
}
} // namespace

void enhanceRawDetails(LinearOutput &image, double amount, CancellationToken stop,
                       const std::function<void(double)> &progress) {
    image.validate();
    if (!std::isfinite(amount) || amount < 0 || amount > 1)
        throw std::runtime_error("Raw Details amount must be between 0 and 1.");
    if (amount == 0) {
        if (progress)
            progress(1);
        return;
    }
    const auto count = pixelCount(image.width, image.height);
    std::vector<float> delta(count);
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const auto offset = index * 4;
            const double alpha = image.rgba[offset + 3];
            if (alpha <= 1e-8)
                continue;
            const double center = luminance(image, x, y);
            double weighted = center, total = 1;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    const int sx = std::clamp(x + dx, 0, image.width - 1);
                    const int sy = std::clamp(y + dy, 0, image.height - 1);
                    const auto sample = (static_cast<std::size_t>(sy) * image.width + sx) * 4;
                    const double coverage = image.rgba[sample + 3];
                    if (coverage <= 1e-8)
                        continue;
                    const double neighbor = luminance(image, sx, sy);
                    const double range =
                        .015 + .08 * std::max(std::abs(center), std::abs(neighbor));
                    const double difference = center - neighbor;
                    const double spatial = dx == 0 || dy == 0 ? 1.0 : .7071067811865476;
                    const double weight = coverage * spatial *
                                          std::exp(-.5 * difference * difference / (range * range));
                    weighted += neighbor * weight;
                    total += weight;
                }
            const double residual = center - weighted / total;
            const double limit = .12 * (.1 + std::abs(center));
            delta[index] = static_cast<float>(.55 * amount * std::clamp(residual, -limit, limit));
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.65 * static_cast<double>(y + 1) / image.height);
    }
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const auto offset = index * 4;
            if (image.rgba[offset + 3] <= 1e-8 || delta[index] == 0)
                continue;
            const double value = luminance(image, x, y);
            if (std::abs(value) > 1e-8) {
                const double scale = (value + delta[index]) / value;
                for (std::size_t channel = 0; channel < 3; ++channel)
                    image.rgba[offset + channel] =
                        static_cast<float>(image.rgba[offset + channel] * scale);
            } else {
                for (std::size_t channel = 0; channel < 3; ++channel)
                    image.rgba[offset + channel] += delta[index];
            }
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.65 + .35 * static_cast<double>(y + 1) / image.height);
    }
    image.validate();
}

void recoverLensBlur(LinearOutput &image, double amount, CancellationToken stop,
                     const std::function<void(double)> &progress) {
    image.validate();
    if (!std::isfinite(amount) || amount < 0 || amount > 1)
        throw std::runtime_error("Lens Blur Recovery amount must be between 0 and 1.");
    if (amount == 0) {
        if (progress)
            progress(1);
        return;
    }
    const auto blurred = gaussianLuminance(image, .65 + .85 * amount, stop, progress);
    const double gain = .55 + 1.25 * amount;
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const auto pixel = index * 4;
            if (image.rgba[pixel + 3] <= 1e-8F)
                continue;
            const double center = luminance(image, x, y);
            const double residual = center - blurred[index];
            const double threshold =
                (.002 + .008 * std::sqrt(std::max(0.0, std::abs(center)))) * (1.15 - .65 * amount);
            const double recovered =
                std::copysign(std::max(0.0, std::abs(residual) - threshold), residual);
            const double limit = .10 * (.05 + std::abs(center));
            const double delta = std::clamp(gain * recovered, -limit, limit);
            for (std::size_t channel = 0; channel < 3; ++channel)
                image.rgba[pixel + channel] += static_cast<float>(delta);
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.9 + .1 * static_cast<double>(y + 1) / image.height);
    }
    image.validate();
}

void suppressColorMoire(LinearOutput &image, double amount, CancellationToken stop,
                        const std::function<void(double)> &progress) {
    image.validate();
    if (!std::isfinite(amount) || amount < 0 || amount > 1)
        throw std::runtime_error("Color Moire amount must be between 0 and 1.");
    if (amount == 0) {
        if (progress)
            progress(1);
        return;
    }
    const auto count = pixelCount(image.width, image.height);
    std::vector<float> correctedU(count), correctedV(count);
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const auto pixel = index * 4;
            const double centerY = luminance(image, x, y);
            const double centerU = image.rgba[pixel] - centerY;
            const double centerV = image.rgba[pixel + 2] - centerY;
            double sumU = centerU, sumV = centerV, total = 1, gradient = 0;
            for (int offsetY = -1; offsetY <= 1; ++offsetY)
                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    if (offsetX == 0 && offsetY == 0)
                        continue;
                    const int sampleX = std::clamp(x + offsetX, 0, image.width - 1);
                    const int sampleY = std::clamp(y + offsetY, 0, image.height - 1);
                    const auto sample =
                        (static_cast<std::size_t>(sampleY) * image.width + sampleX) * 4;
                    const double coverage = image.rgba[sample + 3];
                    if (coverage <= 1e-8)
                        continue;
                    const double sampleYValue = luminance(image, sampleX, sampleY);
                    const double difference = sampleYValue - centerY;
                    gradient = std::max(gradient, std::abs(difference));
                    const double range =
                        .015 + .08 * std::max(std::abs(centerY), std::abs(sampleYValue));
                    const double spatial = offsetX == 0 || offsetY == 0 ? 1.0 : .7071067811865476;
                    const double weight = coverage * spatial *
                                          std::exp(-.5 * difference * difference / (range * range));
                    sumU += (image.rgba[sample] - sampleYValue) * weight;
                    sumV += (image.rgba[sample + 2] - sampleYValue) * weight;
                    total += weight;
                }
            const double averageU = sumU / total, averageV = sumV / total;
            const double residual = std::hypot(centerU - averageU, centerV - averageV);
            const double scale = .02 + .08 * std::max(.05, std::abs(centerY));
            const double flatGate = 1 - std::clamp(gradient / scale, 0.0, 1.0);
            const double frequencyGate = std::clamp((residual - (.003 + .01 * std::abs(centerY))) /
                                                        (.025 + .05 * std::abs(centerY)),
                                                    0.0, 1.0);
            const double blend = amount * flatGate * frequencyGate;
            correctedU[index] = static_cast<float>(centerU + blend * (averageU - centerU));
            correctedV[index] = static_cast<float>(centerV + blend * (averageV - centerV));
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.7 * static_cast<double>(y + 1) / image.height);
    }
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const auto pixel = index * 4;
            if (image.rgba[pixel + 3] <= 1e-8F)
                continue;
            const double valueY = luminance(image, x, y);
            const double red = valueY + correctedU[index];
            const double blue = valueY + correctedV[index];
            const double green = (valueY - .2126 * red - .0722 * blue) / .7152;
            image.rgba[pixel] = static_cast<float>(red);
            image.rgba[pixel + 1] = static_cast<float>(green);
            image.rgba[pixel + 2] = static_cast<float>(blue);
        }
        if (progress && (y % std::max(1, image.height / 100) == 0 || y + 1 == image.height))
            progress(.7 + .3 * static_cast<double>(y + 1) / image.height);
    }
    image.validate();
}

LinearOutput upscaleDetail2x(LinearOutput input, CancellationToken stop,
                             const std::function<void(double)> &progress) {
    input.validate();
    if (input.width > 10000 || input.height > 10000 ||
        static_cast<std::size_t>(input.width) * input.height > 25'000'000)
        throw std::runtime_error("2x Detail exceeds the 100 MP output budget.");
    pixelCount(input.width * 2, input.height * 2);
    const auto horizontal = lanczosWeights(input.width);
    const auto vertical = lanczosWeights(input.height);
    LinearOutput output(input.width * 2, input.height * 2);
    std::map<int, std::vector<float>> rows;
    for (int y = 0; y < output.height; ++y) {
        checkCancellation(stop);
        const auto &weights = vertical[static_cast<std::size_t>(y)];
        int firstNeeded = input.height;
        for (const auto &[sample, weight] : weights) {
            (void)weight;
            firstNeeded = std::min(firstNeeded, sample);
            if (!rows.contains(sample))
                rows.emplace(sample, horizontalRow(input, sample, horizontal, stop));
        }
        for (auto entry = rows.begin(); entry != rows.end();) {
            if (entry->first < firstNeeded)
                entry = rows.erase(entry);
            else
                ++entry;
        }
        for (int x = 0; x < output.width; ++x) {
            const auto target = (static_cast<std::size_t>(y) * output.width + x) * 4;
            for (const auto &[sample, weight] : weights) {
                const auto &row = rows.at(sample);
                const auto source = static_cast<std::size_t>(x) * 4;
                for (std::size_t channel = 0; channel < 4; ++channel)
                    output.rgba[target + channel] +=
                        static_cast<float>(row[source + channel] * weight);
            }
            auto &alpha = output.rgba[target + 3];
            const float filteredAlpha = alpha;
            alpha = std::clamp(filteredAlpha, 0.F, 1.F);
            for (std::size_t channel = 0; channel < 3; ++channel)
                output.rgba[target + channel] =
                    filteredAlpha > 1e-8F ? output.rgba[target + channel] / filteredAlpha : 0;
        }
        if (progress && (y % std::max(1, output.height / 200) == 0 || y + 1 == output.height))
            progress(static_cast<double>(y + 1) / output.height);
    }
    output.validate();
    return output;
}
} // namespace luma
