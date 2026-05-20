#include "SpatialNormalizationService.h"
#include "../common/NiftiIO.h"
#include "../common/ImageOps.h"
#include "../normalizers/RigidVoxelMorphNormalizer.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace Pipeline {

SpatialNormalizationService::SpatialNormalizationService(ConfigurationPtr config)
    : config_(std::move(config)) {
    if (!config_) {
        throw std::invalid_argument("SpatialNormalizationService requires a valid configuration");
    }
}

SpatialNormalizationOutput SpatialNormalizationService::normalize(const SpatialNormalizationRequest& request) {
    SpatialNormalizationOutput output;
    ImageType::Pointer inputImage = loadInput(request.inputPath);

    if (request.skip) {
        output.rigidAlignedImage = inputImage;
        output.spatiallyNormalizedImage = inputImage;
    } else {
        auto normalizer = std::make_shared<RigidVoxelMorphNormalizer>(config_);
        if (request.options.enableDebugOutput) {
            normalizer->setDebugMode(true, request.options.debugOutputBasePath);
        }

        if (request.options.useManualFOV) {
            output.rigidAlignedImage = inputImage;
            output.spatiallyNormalizedImage = normalizer->normalizeManualFOV(inputImage);
        } else if (request.options.useIterativeRigid) {
            auto normResult = normalizer->normalizeIterativeWithIntermediateResults(
                inputImage,
                request.options.maxIterations,
                request.options.convergenceThreshold);
            output.rigidAlignedImage = normResult.rigidAlignedImage;
            output.spatiallyNormalizedImage = request.options.rigidOnly
                                                 ? normResult.rigidAlignedImage
                                                 : normResult.spatiallyNormalizedImage;
        } else {
            auto normResult = normalizer->normalizeWithIntermediateResults(inputImage);
            output.rigidAlignedImage = normResult.rigidAlignedImage;
            output.spatiallyNormalizedImage = request.options.rigidOnly
                                                 ? normResult.rigidAlignedImage
                                                 : normResult.spatiallyNormalizedImage;
        }
    }

    if (request.options.enableAdniPetCore && !request.options.rigidOnly) {
        output.spatiallyNormalizedImage = prepareAdniPetCoreImage(
            output.rigidAlignedImage,
            output.spatiallyNormalizedImage);
    }

    return output;
}

ImageType::Pointer SpatialNormalizationService::loadInput(const std::string& inputPath) const {
    ImageType::Pointer image = Common::nifti::loadImage(inputPath);
    if (!image) {
        throw std::runtime_error("Failed to load input image: " + inputPath);
    }
    return image;
}

ImageType::Pointer SpatialNormalizationService::prepareAdniPetCoreImage(ImageType::Pointer rigidImage,
                                                                       ImageType::Pointer normalizedImage) const {
    if (!rigidImage || !normalizedImage) {
        throw std::invalid_argument("ADNI PET Core preparation requires rigid and normalized images");
    }

    ImageType::Pointer cerebellarGray = Common::nifti::loadImage(config_->getMaskPath("cerebral_gray"));
    if (!cerebellarGray) {
        throw std::runtime_error("Failed to load cerebellar gray mask");
    }

    ImageType::Pointer resampled = Common::image::resampleToMatch(cerebellarGray, normalizedImage);
    double meanGray = Common::image::calculateMeanInMask(resampled, cerebellarGray);
    if (meanGray <= 0.0) {
        throw std::runtime_error("Invalid cerebellar gray mean value for ADNI PET Core normalization");
    }

    ImageType::Pointer adniTemplate = Common::nifti::loadImage(config_->getTemplatePath("adni_pet_core"));
    if (!adniTemplate) {
        throw std::runtime_error("Failed to load ADNI PET Core template");
    }

    ImageType::Pointer adniStyle = Common::image::resampleToMatch(adniTemplate, rigidImage);
    Common::image::divideVoxelsByValue(adniStyle, static_cast<float>(meanGray));
    return adniStyle;
}

} // namespace Pipeline
