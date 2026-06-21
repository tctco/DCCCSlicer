#include "CentiloidCalculator.h"

#include "../suvr/SUVrCalculator.h"
#include <stdexcept>
#include <string>

Pipeline::Metrics::MetricResult CentiloidCalculator::calculate(const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("CentiloidCalculator requires a spatially normalized image");
    }
    if (input.voiMaskPath.empty() || input.refMaskPath.empty()) {
        throw std::invalid_argument("CentiloidCalculator requires VOI and reference masks");
    }

    const double suvr = SUVrCalculator::calculateSUVr(
        input.spatiallyNormalizedImage, input.voiMaskPath, input.refMaskPath);

    Pipeline::Metrics::MetricResult result;
    result.metricName = "Centiloid";
    result.suvr = suvr;

    for (const auto& [tracerName, params] : input.tracerParameters) {
        result.tracerValues[tracerName] = suvr * params.slope + params.intercept;
    }

    return result;
}

std::vector<std::string> CentiloidCalculator::supportedTracers() {
    return {"PiB", "FBP", "FBB", "FMM", "NAV"};
}
