#include "RigidAlignmentNormalizer.h"

#include "../common/Common.h"
#include "../preprocessing/ImagePreprocessor.h"

#include <cmath>
#include <stdexcept>

RigidAlignmentNormalizer::RigidAlignmentNormalizer(ConfigurationPtr config)
    : config_(config) {
    if (!config_) {
        throw std::invalid_argument("RigidAlignmentNormalizer requires configuration");
    }

    rigidEngine_ =
        std::make_unique<RigidRegistrationEngine>(config_->getModelPath("rigid"));
    paddedTemplate_ = Common::nifti::loadImage(config_->getTemplatePath("padded"));
}

ImageType::Pointer RigidAlignmentNormalizer::align(ImageType::Pointer inputImage) {
    ImageType::Pointer rigidImage = performAlignment(inputImage);
    saveDebugImage(rigidImage, "rigid");
    return rigidImage;
}

ImageType::Pointer RigidAlignmentNormalizer::alignIterative(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    ImageType::Pointer currentImage = performAlignment(inputImage, false);
    saveDebugImage(currentImage, "rigid0");
    ImageType::PointType lastOrigin = currentImage->GetOrigin();

    for (int i = 0; i < maxIter; ++i) {
        currentImage = performAlignment(currentImage, true);
        saveDebugImage(currentImage, "rigid" + std::to_string(i + 1));

        float originShift = 0;
        for (int j = 0; j < 3; ++j) {
            originShift +=
                std::pow(currentImage->GetOrigin()[j] - lastOrigin[j], 2);
        }
        originShift = std::sqrt(originShift);

        if (originShift < threshold) {
            break;
        }
        lastOrigin = currentImage->GetOrigin();
    }

    return currentImage;
}

RigidAlignmentNormalizer::AlignmentEstimate RigidAlignmentNormalizer::estimate(
    ImageType::Pointer inputImage, bool resampleFirst) {
    ImageType::Pointer processedImage = inputImage;

    if (resampleFirst) {
        processedImage = Common::image::resampleToMatch(paddedTemplate_, processedImage);
    }

    processedImage = ImagePreprocessor::preprocessForRigid(processedImage);
    saveDebugImage(processedImage, "rigid_preprocessed");

    std::vector<float> imageData;
    Common::image::extractImageData(processedImage, imageData);

    auto orientation = rigidEngine_->predict(imageData, {1, 1, 64, 64, 64});
    return AlignmentEstimate{processedImage, orientation};
}

void RigidAlignmentNormalizer::apply(ImageType::Pointer targetImage,
                                     const AlignmentEstimate& estimate) {
    if (!targetImage) {
        return;
    }

    auto newOriginAndDirection = rigidEngine_->getNewOriginAndDirection(
        estimate.preprocessedImage,
        targetImage,
        estimate.orientation.at("ac"),
        estimate.orientation.at("pa"),
        estimate.orientation.at("is"));

    targetImage->SetOrigin(std::get<0>(newOriginAndDirection));
    targetImage->SetDirection(std::get<1>(newOriginAndDirection));
}

ImageType::Pointer RigidAlignmentNormalizer::performAlignment(
    ImageType::Pointer inputImage, bool resampleFirst) {
    auto alignmentEstimate = estimate(inputImage, resampleFirst);
    apply(inputImage, alignmentEstimate);
    return inputImage;
}

void RigidAlignmentNormalizer::setDebugMode(bool enable, const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
}

void RigidAlignmentNormalizer::saveDebugImage(ImageType::Pointer image,
                                              const std::string& suffix) {
    if (!debugMode_ || debugBasePath_.empty()) {
        return;
    }
    Common::nifti::saveImage(image, debugBasePath_ + "_" + suffix + ".nii");
}
