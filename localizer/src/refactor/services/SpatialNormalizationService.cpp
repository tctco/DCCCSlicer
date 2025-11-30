#include "SpatialNormalizationService.h"
#include "../common/Common.h"
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
        return output;
    }

    auto normalizer = provider_->createRigidVoxelMorphNormalizer();
    if (request.options.enableDebugOutput) {
        normalizer->setDebugMode(true, request.options.debugOutputBasePath);
    }

    if (request.options.useManualFOV) {
        output.rigidAlignedImage = inputImage;
        output.spatiallyNormalizedImage = normalizer->normalizeManualFOV(inputImage);
        return output;
    }

    if (request.options.useIterativeRigid) {
        auto normResult = normalizer->normalizeIterativeWithIntermediateResults(
            inputImage,
            request.options.maxIterations,
            request.options.convergenceThreshold);
        output.rigidAlignedImage = normResult.rigidAlignedImage;
        output.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
        return output;
    }

    auto normResult = normalizer->normalizeWithIntermediateResults(inputImage);
    output.rigidAlignedImage = normResult.rigidAlignedImage;
    output.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
    return output;
}

ImageType::Pointer SpatialNormalizationService::loadInput(const std::string& inputPath) const {
    ImageType::Pointer image = refactorCommon::nifti::loadImage(inputPath);
    if (!image) {
        throw std::runtime_error("Failed to load input image: " + inputPath);
    }
    return image;
}

} // namespace RefactorPipeline

