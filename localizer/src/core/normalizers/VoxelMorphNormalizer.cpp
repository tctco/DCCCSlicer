#include "VoxelMorphNormalizer.h"

#include "../common/Common.h"
#include "../preprocessing/ImagePreprocessor.h"

#include <itkRegionOfInterestImageFilter.h>
#include <stdexcept>
#include <vector>

VoxelMorphNormalizer::VoxelMorphNormalizer(ConfigurationPtr config)
    : config_(config) {
    if (!config_) {
        throw std::invalid_argument("VoxelMorphNormalizer requires configuration");
    }

    nonlinearEngine_ = std::make_unique<NonlinearRegistrationEngine>(
        config_->getModelPath("affine_voxelmorph"));
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
