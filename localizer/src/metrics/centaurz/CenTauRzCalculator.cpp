#include "CenTauRzCalculator.h"

#include "../suvr/SUVrCalculator.h"
#include <stdexcept>
#include <string>

Pipeline::Metrics::MetricResult CenTauRzCalculator::calculate(const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("CenTauRzCalculator requires a spatially normalized image");
    }
    if (input.voiMaskPath.empty() || input.refMaskPath.empty()) {
        throw std::invalid_argument("CenTauRzCalculator requires VOI and reference masks");
    }

    const double suvr = SUVrCalculator::calculateSUVr(
        input.spatiallyNormalizedImage, input.voiMaskPath, input.refMaskPath);

    Pipeline::Metrics::MetricResult result;
    result.metricName = "CenTauRz";
    result.suvr = suvr;

    for (const auto& [tracerName, params] : input.tracerParameters) {
        float centaurz = suvr * params.slope + params.intercept;
        result.tracerValues[tracerName] = centaurz;
    }
    return result;
}
