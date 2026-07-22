#include "RigidAlignmentNormalizer.h"

#include "../common/Common.h"
#include "../preprocessing/ImagePreprocessor.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::string describeImage(ImageType::Pointer image) {
    if (!image) {
        return "image=null";
    }

    const auto size = image->GetLargestPossibleRegion().GetSize();
    const auto spacing = image->GetSpacing();
    std::ostringstream stream;
    stream << "size=" << size[0] << "x" << size[1] << "x" << size[2]
           << " spacing=" << spacing[0] << "x" << spacing[1] << "x"
           << spacing[2];
    return stream.str();
}

}  // namespace

RigidAlignmentNormalizer::RigidAlignmentNormalizer(ConfigurationPtr config)
    : RigidAlignmentNormalizer(config, nullptr) {}

RigidAlignmentNormalizer::RigidAlignmentNormalizer(
    ConfigurationPtr config, Common::debug::DebugReporterPtr debugReporter)
    : config_(config), debugReporter_(std::move(debugReporter)) {
    if (!config_) {
        throw std::invalid_argument("RigidAlignmentNormalizer requires configuration");
    }

    {
        const std::string modelPath = config_->getModelPath("rigid");
        auto scope = debugReporter_ ? debugReporter_->scope("load_rigid_model", modelPath)
                                    : Common::debug::ScopedStage{};
        rigidEngine_ =
            std::make_unique<RigidRegistrationEngine>(modelPath, debugReporter_);
    }
    {
        const std::string templatePath = config_->getTemplatePath("padded");
        auto scope = debugReporter_ ? debugReporter_->scope("load_padded_template", templatePath)
                                    : Common::debug::ScopedStage{};
        paddedTemplate_ = Common::nifti::loadImage(templatePath);
        if (debugReporter_) {
            debugReporter_->event("padded_template", describeImage(paddedTemplate_));
        }
    }
}

ImageType::Pointer RigidAlignmentNormalizer::align(ImageType::Pointer inputImage) {
    auto scope = debugReporter_ ? debugReporter_->scope("rigid.align", describeImage(inputImage))
                                : Common::debug::ScopedStage{};
    ImageType::Pointer rigidImage = performAlignment(inputImage, false, "rigid.single");
    saveDebugImage(rigidImage, "rigid");
    return rigidImage;
}

ImageType::Pointer RigidAlignmentNormalizer::alignIterative(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    auto scope = debugReporter_
                     ? debugReporter_->scope("rigid.align_iterative",
                                             describeImage(inputImage) +
                                                 " max_iter=" + std::to_string(maxIter) +
                                                 " threshold=" + std::to_string(threshold))
                     : Common::debug::ScopedStage{};
    ImageType::Pointer currentImage =
        performAlignment(inputImage, false, "rigid.iteration0");
    saveDebugImage(currentImage, "rigid0");
    ImageType::PointType lastOrigin = currentImage->GetOrigin();

    for (int i = 0; i < maxIter; ++i) {
        const std::string stageLabel = "rigid.iteration" + std::to_string(i + 1);
        currentImage = performAlignment(currentImage, true, stageLabel);
        saveDebugImage(currentImage, "rigid" + std::to_string(i + 1));

        float originShift = 0;
        for (int j = 0; j < 3; ++j) {
            originShift +=
                std::pow(currentImage->GetOrigin()[j] - lastOrigin[j], 2);
        }
        originShift = std::sqrt(originShift);
        if (debugReporter_) {
            debugReporter_->event(stageLabel + ".origin_shift",
                                  "mm=" + std::to_string(originShift));
        }

        if (originShift < threshold) {
            if (debugReporter_) {
                debugReporter_->event(stageLabel + ".converged",
                                      "threshold=" + std::to_string(threshold));
            }
            break;
        }
        lastOrigin = currentImage->GetOrigin();
    }

    return currentImage;
}

RigidAlignmentNormalizer::AlignmentEstimate RigidAlignmentNormalizer::estimate(
    ImageType::Pointer inputImage, bool resampleFirst, const std::string& stageLabel) {
    auto scope = debugReporter_ ? debugReporter_->scope(stageLabel + ".estimate")
                                : Common::debug::ScopedStage{};
    ImageType::Pointer processedImage = inputImage;

    if (resampleFirst) {
        auto resampleScope =
            debugReporter_
                ? debugReporter_->scope(stageLabel + ".resample_to_padded_template",
                                        describeImage(processedImage))
                : Common::debug::ScopedStage{};
        processedImage = Common::image::resampleToMatch(paddedTemplate_, processedImage);
        if (debugReporter_) {
            debugReporter_->event(stageLabel + ".resampled_image",
                                  describeImage(processedImage));
        }
    }

    processedImage =
        ImagePreprocessor::preprocessForRigid(
            processedImage, debugReporter_.get(), stageLabel + ".preprocess");
    saveDebugImage(processedImage, "rigid_preprocessed");

    std::vector<float> imageData;
    {
        auto extractScope = debugReporter_
                                ? debugReporter_->scope(stageLabel + ".extract_tensor_data",
                                                        describeImage(processedImage))
                                : Common::debug::ScopedStage{};
        Common::image::extractImageData(processedImage, imageData);
    }

    auto orientation = rigidEngine_->predict(imageData, {1, 1, 64, 64, 64});
    return AlignmentEstimate{processedImage, orientation};
}

void RigidAlignmentNormalizer::apply(ImageType::Pointer targetImage,
                                     const AlignmentEstimate& estimate,
                                     const std::string& stageLabel) {
    if (!targetImage) {
        return;
    }

    auto scope = debugReporter_ ? debugReporter_->scope(stageLabel + ".apply_transform")
                                : Common::debug::ScopedStage{};
    auto newOriginAndDirection =
        rigidEngine_->getNewOriginAndDirection(estimate.preprocessedImage,
                                               targetImage,
                                               estimate.orientation.at("ac"),
                                               estimate.orientation.at("pa"),
                                               estimate.orientation.at("is"));

    targetImage->SetOrigin(std::get<0>(newOriginAndDirection));
    targetImage->SetDirection(std::get<1>(newOriginAndDirection));
}

ImageType::Pointer RigidAlignmentNormalizer::performAlignment(
    ImageType::Pointer inputImage, bool resampleFirst, const std::string& stageLabel) {
    auto scope = debugReporter_ ? debugReporter_->scope(stageLabel + ".perform_alignment",
                                                        "resample_first=" +
                                                            std::string(resampleFirst ? "true"
                                                                                      : "false"))
                                : Common::debug::ScopedStage{};
    auto alignmentEstimate = estimate(inputImage, resampleFirst, stageLabel);
    apply(inputImage, alignmentEstimate, stageLabel);
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
    const std::string outputPath = debugBasePath_ + "_" + suffix + ".nii";
    auto scope = debugReporter_ ? debugReporter_->scope("save_debug_image", outputPath)
                                : Common::debug::ScopedStage{};
    Common::nifti::saveImage(image, outputPath);
}
