#include "SUVrCalculator.h"

#include "../../core/common/Common.h"
#include <stdexcept>
#include <string>

Pipeline::Metrics::MetricResult SUVrCalculator::calculate(const Input& input) const {
    if (!input.spatiallyNormalizedImage) {
        throw std::invalid_argument("SUVrCalculator requires a spatially normalized image");
    }
    if (input.voiMaskPath.empty() || input.refMaskPath.empty()) {
        throw std::invalid_argument("SUVrCalculator requires VOI and reference masks");
    }

    const double suvr = calculateSUVr(
        input.spatiallyNormalizedImage, input.voiMaskPath, input.refMaskPath);

    Pipeline::Metrics::MetricResult result;
    result.metricName = "SUVr";
    result.suvr = suvr;
    result.tracerValues["SUVr"] = static_cast<float>(suvr);
    return result;
}

double SUVrCalculator::calculateSUVr(ImageType::Pointer spatialNormalizedImage,
                                     const std::string& voiMaskPath,
                                     const std::string& refMaskPath) {
    ImageType::Pointer voiTemplate = Common::nifti::loadImage(voiMaskPath);
    ImageType::Pointer refTemplate = Common::nifti::loadImage(refMaskPath);
    ImageType::Pointer resampledImage = Common::image::resampleToMatch(voiTemplate, spatialNormalizedImage);
    
    double meanVoi = Common::image::calculateMeanInMask(resampledImage, voiTemplate);
    double meanRef = Common::image::calculateMeanInMask(resampledImage, refTemplate);

    return meanVoi / meanRef;
}
