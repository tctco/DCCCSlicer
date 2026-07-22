#include "RigidVoxelMorphNormalizer.h"

#include <stdexcept>
#include <utility>

RigidVoxelMorphNormalizer::RigidVoxelMorphNormalizer(ConfigurationPtr config)
    : RigidVoxelMorphNormalizer(config, nullptr) {}

RigidVoxelMorphNormalizer::RigidVoxelMorphNormalizer(
    ConfigurationPtr config, Common::debug::DebugReporterPtr debugReporter)
    : config_(config), debugReporter_(std::move(debugReporter)) {
    if (!config_) {
        throw std::invalid_argument("RigidVoxelMorphNormalizer requires configuration");
    }
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalize(ImageType::Pointer inputImage) {
    ImageType::Pointer rigidImage = rigidNormalizer().align(inputImage);
    return voxelMorphNormalizer().normalize(rigidImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeRigidOnly(
    ImageType::Pointer inputImage) {
    return rigidNormalizer().align(inputImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeIterativeRigidOnly(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    return rigidNormalizer().alignIterative(inputImage, maxIter, threshold);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeIterative(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    ImageType::Pointer rigidImage =
        rigidNormalizer().alignIterative(inputImage, maxIter, threshold);
    return voxelMorphNormalizer().normalize(rigidImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeManualFOV(
    ImageType::Pointer inputImage) {
    return voxelMorphNormalizer().normalize(inputImage);
}

RigidVoxelMorphNormalizer::NormalizationResult
RigidVoxelMorphNormalizer::normalizeWithIntermediateResults(ImageType::Pointer inputImage) {
    NormalizationResult result;
    result.rigidAlignedImage = rigidNormalizer().align(inputImage);
    result.spatiallyNormalizedImage =
        voxelMorphNormalizer().normalize(result.rigidAlignedImage);
    return result;
}

RigidVoxelMorphNormalizer::NormalizationResult
RigidVoxelMorphNormalizer::normalizeIterativeWithIntermediateResults(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    NormalizationResult result;
    result.rigidAlignedImage =
        rigidNormalizer().alignIterative(inputImage, maxIter, threshold);
    result.spatiallyNormalizedImage =
        voxelMorphNormalizer().normalize(result.rigidAlignedImage);
    return result;
}

std::string RigidVoxelMorphNormalizer::getName() const {
    return "RigidVoxelMorph";
}

bool RigidVoxelMorphNormalizer::isSupported(const std::string&) const {
    return true;
}

void RigidVoxelMorphNormalizer::setDebugMode(bool enable,
                                             const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
    if (enable && !debugReporter_) {
        debugReporter_ = std::make_shared<Common::debug::DebugReporter>("rigid");
    }
    if (rigidNormalizer_) {
        rigidNormalizer_->setDebugMode(enable, basePath);
    }
    if (voxelMorphNormalizer_) {
        voxelMorphNormalizer_->setDebugMode(enable, basePath);
    }
}

RigidAlignmentNormalizer& RigidVoxelMorphNormalizer::rigidNormalizer() {
    if (!rigidNormalizer_) {
        auto scope = debugReporter_ ? debugReporter_->scope("create_rigid_normalizer")
                                    : Common::debug::ScopedStage{};
        rigidNormalizer_ =
            std::make_unique<RigidAlignmentNormalizer>(config_, debugReporter_);
        rigidNormalizer_->setDebugMode(debugMode_, debugBasePath_);
    }
    return *rigidNormalizer_;
}

VoxelMorphNormalizer& RigidVoxelMorphNormalizer::voxelMorphNormalizer() {
    if (!voxelMorphNormalizer_) {
        voxelMorphNormalizer_ = std::make_unique<VoxelMorphNormalizer>(config_);
        voxelMorphNormalizer_->setDebugMode(debugMode_, debugBasePath_);
    }
    return *voxelMorphNormalizer_;
}
