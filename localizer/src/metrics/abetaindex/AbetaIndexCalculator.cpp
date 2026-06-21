#include "AbetaIndexCalculator.h"

#include "../../core/common/Common.h"
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

std::string normalizeTracer(std::string tracer) {
    for (char& ch : tracer) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return tracer;
}

} // namespace

Pipeline::Metrics::MetricResult AbetaIndexCalculator::calculate(const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("AbetaIndexCalculator requires a spatially normalized image.");
    }

    const std::string tracer = normalizeTracer(input.tracer);
    if (tracer != "AV45") {
        throw std::invalid_argument("AbetaIndex currently supports only the AV45 tracer.");
    }

    ImageType::Pointer meanTemplate = Common::nifti::loadImage(input.meanTemplatePath);
    ImageType::Pointer pc1Template = Common::nifti::loadImage(input.pc1TemplatePath);
    ImageType::Pointer pc2Template = Common::nifti::loadImage(input.pc2TemplatePath);
    if (!meanTemplate || !pc1Template || !pc2Template) {
        throw std::runtime_error("Failed to load AbetaIndex templates.");
    }

    meanTemplate = Common::image::resampleToMatch(input.spatiallyNormalizedImage, meanTemplate);
    pc1Template = Common::image::resampleToMatch(input.spatiallyNormalizedImage, pc1Template);
    pc2Template = Common::image::resampleToMatch(input.spatiallyNormalizedImage, pc2Template);

    std::vector<float> imageData;
    std::vector<float> meanData;
    std::vector<float> pc1Data;
    std::vector<float> pc2Data;
    Common::image::extractImageData(input.spatiallyNormalizedImage, imageData);
    Common::image::extractImageData(meanTemplate, meanData);
    Common::image::extractImageData(pc1Template, pc1Data);
    Common::image::extractImageData(pc2Template, pc2Data);

    if (imageData.size() != meanData.size() || imageData.size() != pc1Data.size() ||
        imageData.size() != pc2Data.size()) {
        throw std::runtime_error("AbetaIndex inputs have mismatched dimensions after resampling.");
    }

    double numerator = 0.0;
    double denominator = 0.0;
    for (size_t i = 0; i < imageData.size(); ++i) {
        const double imageValue = static_cast<double>(imageData[i]);
        const double meanValue = static_cast<double>(meanData[i]);
        const double pc1Value = static_cast<double>(pc1Data[i]);
        const double pc2Value = static_cast<double>(pc2Data[i]);
        if (!std::isfinite(imageValue) || !std::isfinite(meanValue) || !std::isfinite(pc1Value) ||
            !std::isfinite(pc2Value)) {
            continue;
        }

        const double centeredResidual = imageValue - meanValue - pc2Value;
        numerator += pc1Value * centeredResidual;
        denominator += pc1Value * pc1Value;
    }

    if (denominator <= 0.0) {
        throw std::runtime_error("AbetaIndex PC1 template has no usable support.");
    }

    const double abetaIndex = numerator / denominator;

    Pipeline::Metrics::MetricResult result;
    result.metricName = "AbetaIndex";
    result.suvr = abetaIndex;
    result.tracerValues["AV45"] = static_cast<float>(abetaIndex);
    return result;
}
