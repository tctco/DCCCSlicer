#include "SpatialNormalizationService.h"
#include "../common/DebugReporter.h"
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
    const auto& debugReporter = request.options.debugReporter;
    ImageType::Pointer inputImage;
    {
        auto scope = debugReporter ? debugReporter->scope("load_input", request.inputPath)
                                   : Common::debug::ScopedStage{};
        inputImage = loadInput(request.inputPath);
    }

    if (request.skip) {
        output.rigidAlignedImage = inputImage;
        output.spatiallyNormalizedImage = inputImage;
    } else {
        auto normalizer = std::make_shared<RigidVoxelMorphNormalizer>(config_, debugReporter);
        if (request.options.enableDebugOutput) {
            normalizer->setDebugMode(true, request.options.debugOutputBasePath);
        }

        if (request.options.rigidOnly) {
            auto scope = debugReporter ? debugReporter->scope("normalize.rigid_only")
                                       : Common::debug::ScopedStage{};
            output.rigidAlignedImage = request.options.useIterativeRigid
                                           ? normalizer->normalizeIterativeRigidOnly(
                                                 inputImage,
                                                 request.options.maxIterations,
                                                 request.options.convergenceThreshold)
                                           : normalizer->normalizeRigidOnly(inputImage);
            output.spatiallyNormalizedImage = output.rigidAlignedImage;
        } else if (request.options.useManualFOV) {
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

    if (request.options.enableAdniPetCore && !request.options.rigidOnly) {
        output.spatiallyNormalizedImage = prepareAdniPetCoreImage(
            output.rigidAlignedImage,
            output.spatiallyNormalizedImage,
            request.options.adniPetTracer);
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
                                                                       ImageType::Pointer normalizedImage,
                                                                       const std::string& tracer) const {
    if (!rigidImage || !normalizedImage) {
        throw std::invalid_argument("ADNI PET Core preparation requires rigid and normalized images");
    }

    if (tracer != "abeta" && tracer != "tau" && tracer != "fdg" &&
        tracer != "dat") {
        throw std::invalid_argument(
            "ADNI PET Core tracer must be one of: abeta, tau, fdg, dat");
    }

    ImageType::Pointer adniTemplate = Common::nifti::loadImage(config_->getTemplatePath("adni_pet_core"));
    if (!adniTemplate) {
        throw std::runtime_error("Failed to load ADNI PET Core template");
    }

    ImageType::Pointer adniStyle = Common::image::resampleToMatch(adniTemplate, rigidImage);
    if (tracer == "fdg") {
        Common::image::normalizeFdgIterativeGlobalMean(adniStyle);
        return adniStyle;
    }

    const bool useOccipitalReference = tracer == "dat";
    const std::string referenceMaskName =
        useOccipitalReference ? "dat_occipital_ref" : "cerebral_gray";
    ImageType::Pointer referenceMask =
        Common::nifti::loadImage(config_->getMaskPath(referenceMaskName));
    if (!referenceMask) {
        throw std::runtime_error(
            useOccipitalReference
                ? "Failed to load DAT occipital reference mask"
                : "Failed to load cerebellar gray mask");
    }

    ImageType::Pointer resampled =
        Common::image::resampleToMatch(referenceMask, normalizedImage);
    const double referenceMean =
        Common::image::calculateMeanInMask(resampled, referenceMask);
    if (referenceMean <= 0.0) {
        throw std::runtime_error(
            useOccipitalReference
                ? "Invalid occipital mean value for DAT PPMI-style normalization"
                : "Invalid cerebellar gray mean value for ADNI PET Core normalization");
    }
    Common::image::divideVoxelsByValue(
        adniStyle, static_cast<float>(referenceMean));
    return adniStyle;
}

} // namespace Pipeline
