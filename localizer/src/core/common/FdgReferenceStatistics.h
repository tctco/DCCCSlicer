#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

namespace Common::image {

// SCAN-inspired reference: mean of the highest ceil(N / 2) samples in the
// combined pons/vermis ROI, not the average of two independent ROI means.
inline double fdgTopHalfMean(std::vector<float> samples) {
    if (samples.empty()) {
        throw std::runtime_error("FDG pons/vermis reference mask contains no voxels");
    }
    for (const float value : samples) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("FDG pons/vermis reference contains non-finite values");
        }
    }
    const std::size_t retained = samples.size() / 2 + samples.size() % 2;
    std::partial_sort(samples.begin(), samples.begin() + retained,
                      samples.end(), std::greater<float>());
    double sum = 0.0;
    for (std::size_t i = 0; i < retained; ++i) {
        sum += samples[i];
    }
    const double mean = sum / static_cast<double>(retained);
    if (!std::isfinite(mean) || mean <= 0.0) {
        throw std::runtime_error("Invalid FDG pons/vermis top-half reference mean");
    }
    return mean;
}

} // namespace Common::image
