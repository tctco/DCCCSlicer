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

    return calculateRegion(
        "CenTauRz",
        input.spatiallyNormalizedImage,
        input.voiMaskPath,
        input.refMaskPath,
        input.tracerParameters);
}

std::vector<Pipeline::Metrics::MetricResult> CenTauRzCalculator::calculateDetailedRegions(
    const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("CenTauRzCalculator requires a spatially normalized image");
    }
    if (input.refMaskPath.empty()) {
        throw std::invalid_argument("CenTauRzCalculator requires a reference mask");
    }

    std::vector<Pipeline::Metrics::MetricResult> results;
    results.reserve(input.detailedRegions.size());
    for (const auto& region : input.detailedRegions) {
        if (region.metricName.empty() || region.voiMaskPath.empty()) {
            throw std::invalid_argument("CenTauRzCalculator detailed regions require metric names and VOI masks");
        }
        results.push_back(calculateRegion(
            region.metricName,
            input.spatiallyNormalizedImage,
            region.voiMaskPath,
            input.refMaskPath,
            region.tracerParameters));
    }
    return results;
}

Pipeline::Metrics::MetricResult CenTauRzCalculator::calculateRegion(
    const std::string& metricName,
    ImageType::Pointer spatiallyNormalizedImage,
    const std::string& voiMaskPath,
    const std::string& refMaskPath,
    const std::map<std::string, TracerParams>& tracerParameters) const {
    const double suvr = SUVrCalculator::calculateSUVr(
        spatiallyNormalizedImage, voiMaskPath, refMaskPath);

    Pipeline::Metrics::MetricResult result;
    result.metricName = metricName;
    result.suvr = suvr;
    for (const auto& [tracerName, params] : tracerParameters) {
        float centaurz = suvr * params.slope + params.intercept;
        result.tracerValues[tracerName] = centaurz;
    }
    return result;
}
