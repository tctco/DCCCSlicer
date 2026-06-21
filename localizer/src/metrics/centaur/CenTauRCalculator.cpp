#include "CenTauRCalculator.h"

#include "../suvr/SUVrCalculator.h"
#include <stdexcept>
#include <string>

Pipeline::Metrics::MetricResult CenTauRCalculator::calculate(const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("CenTauRCalculator requires a spatially normalized image");
    }
    if (input.voiMaskPath.empty() || input.refMaskPath.empty()) {
        throw std::invalid_argument("CenTauRCalculator requires VOI and reference masks");
    }

    const double suvr = SUVrCalculator::calculateSUVr(
        input.spatiallyNormalizedImage, input.voiMaskPath, input.refMaskPath);

    Pipeline::Metrics::MetricResult result;
    result.metricName = "CenTauR";
    result.suvr = suvr;

    for (const auto& [tracerName, params] : input.tracerParameters) {
        float centaur = (suvr - params.baselineSuvr) / (params.maxSuvr - params.baselineSuvr) * 100;
        result.tracerValues[tracerName] = centaur;
    }
    return result;
}
