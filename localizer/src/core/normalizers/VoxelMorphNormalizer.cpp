#include "VoxelMorphNormalizer.h"

#include "../common/Common.h"
#include "../preprocessing/ImagePreprocessor.h"

#include <itkRegionOfInterestImageFilter.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

VoxelMorphNormalizer::VoxelMorphNormalizer(ConfigurationPtr config,
                                           const std::string& modelName)
    : config_(config) {
    if (!config_) {
        throw std::invalid_argument("VoxelMorphNormalizer requires configuration");
    }

    nonlinearEngine_ = std::make_unique<NonlinearRegistrationEngine>(
        config_->getModelPath(modelName));
    paddedTemplate_ = Common::nifti::loadImage(config_->getTemplatePath("padded"));
}

ImageType::Pointer VoxelMorphNormalizer::normalize(ImageType::Pointer rigidImage) {
    ImageType::Pointer paddedImage =
        Common::image::resampleToMatch(paddedTemplate_, rigidImage);

    paddedImage = ImagePreprocessor::preprocessForVoxelMorph(paddedImage);
    saveDebugImage(paddedImage, "elastic_preprocessed");

    std::vector<float> paddedImageData, paddedTemplateData, paddedOriginalData;
    Common::image::extractImageData(paddedImage, paddedImageData);
    Common::image::extractImageData(paddedTemplate_, paddedTemplateData);

    ImageType::Pointer paddedOriginalImage =
        Common::image::resampleToMatch(paddedTemplate_, rigidImage);
    Common::image::extractImageData(paddedOriginalImage, paddedOriginalData);

    auto warpedImageData = nonlinearEngine_->predict(
        paddedOriginalData, paddedImageData, paddedTemplateData);

    ImageType::Pointer warpedImage = Common::image::createImageFromVector(
        warpedImageData["warped"], paddedImage->GetLargestPossibleRegion().GetSize());

    warpedImage->SetDirection(paddedTemplate_->GetDirection());
    warpedImage->SetOrigin(paddedTemplate_->GetOrigin());
    warpedImage->SetSpacing(paddedTemplate_->GetSpacing());

    return cropMNI(warpedImage);
}

ImageType::Pointer VoxelMorphNormalizer::inverseWarp(
    ImageType::Pointer rigidImage, ImageType::Pointer templateImage) {
    auto channels = inverseWarpChannels(rigidImage, {templateImage});
    return channels.front();
}

std::vector<ImageType::Pointer> VoxelMorphNormalizer::inverseWarpChannels(
    ImageType::Pointer rigidImage,
    const std::vector<ImageType::Pointer>& templateImages) {
    if (!rigidImage || templateImages.empty()) {
        throw std::invalid_argument(
            "Inverse VoxelMorph warping requires a rigid image and template images");
    }
    if (std::any_of(templateImages.begin(), templateImages.end(),
                    [](const ImageType::Pointer& image) { return !image; })) {
        throw std::invalid_argument("Inverse VoxelMorph template images cannot be null");
    }
    if (!nonlinearEngine_->supportsInverseWarp()) {
        throw std::runtime_error(
            "The configured affine VoxelMorph model does not expose inverse_warped");
    }

    ImageType::Pointer paddedImage =
        Common::image::resampleToMatch(paddedTemplate_, rigidImage);
    paddedImage = ImagePreprocessor::preprocessForVoxelMorph(paddedImage);
    saveDebugImage(paddedImage, "elastic_preprocessed");

    ImageType::Pointer paddedOriginalImage =
        Common::image::resampleToMatch(paddedTemplate_, rigidImage);
    std::vector<float> paddedImageData;
    std::vector<float> paddedTemplateData;
    std::vector<float> paddedOriginalData;
    std::vector<float> templateImageData;
    Common::image::extractImageData(paddedImage, paddedImageData);
    Common::image::extractImageData(paddedTemplate_, paddedTemplateData);
    Common::image::extractImageData(paddedOriginalImage, paddedOriginalData);
    const std::size_t channelVoxelCount =
        paddedTemplate_->GetLargestPossibleRegion().GetNumberOfPixels();
    templateImageData.reserve(channelVoxelCount * templateImages.size());
    for (const auto& templateImage : templateImages) {
        ImageType::Pointer paddedTemplateImage =
            Common::image::resampleToMatch(paddedTemplate_, templateImage);
        std::vector<float> channelData;
        Common::image::extractImageData(paddedTemplateImage, channelData);
        templateImageData.insert(
            templateImageData.end(), channelData.begin(), channelData.end());
    }

    auto prediction = nonlinearEngine_->predict(
        paddedOriginalData, paddedImageData, paddedTemplateData,
        &templateImageData, templateImages.size());
    const auto inverseIt = prediction.find("inverse_warped");
    if (inverseIt == prediction.end()) {
        throw std::runtime_error("Inverse VoxelMorph prediction did not return inverse_warped");
    }

    if (inverseIt->second.size() != channelVoxelCount * templateImages.size()) {
        throw std::runtime_error(
            "Inverse VoxelMorph output channel count does not match its input");
    }

    std::vector<ImageType::Pointer> inverseImages;
    inverseImages.reserve(templateImages.size());
    for (std::size_t channel = 0; channel < templateImages.size(); ++channel) {
        const auto begin = inverseIt->second.begin() + channel * channelVoxelCount;
        std::vector<float> channelData(begin, begin + channelVoxelCount);
        ImageType::Pointer inverseImage = Common::image::createImageFromVector(
            channelData, paddedTemplate_->GetLargestPossibleRegion().GetSize());
        inverseImage->SetDirection(paddedTemplate_->GetDirection());
        inverseImage->SetOrigin(paddedTemplate_->GetOrigin());
        inverseImage->SetSpacing(paddedTemplate_->GetSpacing());
        saveDebugImage(inverseImage,
                       "inverse_warped_channel_" + std::to_string(channel));
        inverseImages.push_back(inverseImage);
    }
    return inverseImages;
}

ImageType::Pointer VoxelMorphNormalizer::cropMNI(ImageType::Pointer image) {
    ImageType::RegionType cropRegion;
    ImageType::RegionType::IndexType start;
    start[0] = config_->getInt("processing.crop_mni.start_x", 8);
    start[1] = config_->getInt("processing.crop_mni.start_y", 16);
    start[2] = config_->getInt("processing.crop_mni.start_z", 8);

    ImageType::RegionType::SizeType size;
    size[0] = config_->getInt("processing.crop_mni.size_x", 79);
    size[1] = config_->getInt("processing.crop_mni.size_y", 95);
    size[2] = config_->getInt("processing.crop_mni.size_z", 79);

    cropRegion.SetSize(size);
    cropRegion.SetIndex(start);

    using FilterType = itk::RegionOfInterestImageFilter<ImageType, ImageType>;
    FilterType::Pointer filter = FilterType::New();
    filter->SetRegionOfInterest(cropRegion);
    filter->SetInput(image);
    filter->Update();

    return filter->GetOutput();
}

void VoxelMorphNormalizer::setDebugMode(bool enable, const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
}

void VoxelMorphNormalizer::saveDebugImage(ImageType::Pointer image,
                                          const std::string& suffix) {
    if (!debugMode_ || debugBasePath_.empty()) {
        return;
    }
    Common::nifti::saveImage(image, debugBasePath_ + "_" + suffix + ".nii");
}
