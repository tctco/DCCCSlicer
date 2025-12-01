#include "SpatialNormalizationService.h"
#include "../common/NiftiIO.h"
#include "../common/ImageOps.h"
#include <stdexcept>
#include <utility>

namespace RefactorPipeline {

SpatialNormalizationService::SpatialNormalizationService(ConfigurationPtr config,
                                                         std::shared_ptr<LegacyNormalizerProvider> provider)
    : config_(std::move(config)),
      provider_(std::move(provider)) {
    if (!provider_) {
        throw std::invalid_argument("SpatialNormalizationService requires a valid normalizer provider");
    }
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
        auto normalizer = provider_->createRigidVoxelMorphNormalizer();
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
            output.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
        } else {
            auto normResult = normalizer->normalizeWithIntermediateResults(inputImage);
            output.rigidAlignedImage = normResult.rigidAlignedImage;
            output.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
        }
    }

    if (request.options.enableAdniPetCore) {
        output.spatiallyNormalizedImage = prepareAdniPetCoreImage(
            output.rigidAlignedImage,
            output.spatiallyNormalizedImage);
    }

    return output;
}

ImageType::Pointer SpatialNormalizationService::loadInput(const std::string& inputPath) const {
    ImageType::Pointer image = refactorCommon::nifti::loadImage(inputPath);
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

    ImageType::Pointer cerebellarGray = refactorCommon::nifti::loadImage(config_->getMaskPath("cerebral_gray"));
    if (!cerebellarGray) {
        throw std::runtime_error("Failed to load cerebellar gray mask");
    }

    ImageType::Pointer resampled = refactorCommon::image::resampleToMatch(cerebellarGray, normalizedImage);
    double meanGray = refactorCommon::image::calculateMeanInMask(resampled, cerebellarGray);
    if (meanGray <= 0.0) {
        throw std::runtime_error("Invalid cerebellar gray mean value for ADNI PET Core normalization");
    }

    ImageType::Pointer adniTemplate = refactorCommon::nifti::loadImage(config_->getTemplatePath("adni_pet_core"));
    if (!adniTemplate) {
        throw std::runtime_error("Failed to load ADNI PET Core template");
    }

    ImageType::Pointer adniStyle = refactorCommon::image::resampleToMatch(adniTemplate, rigidImage);
    refactorCommon::image::divideVoxelsByValue(adniStyle, static_cast<float>(meanGray));
    return adniStyle;
}

} // namespace RefactorPipeline

