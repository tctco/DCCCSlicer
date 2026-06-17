#include "RigidVoxelMorphNormalizer.h"
#include "../common/Common.h"
#include <itkRegionOfInterestImageFilter.h>

RigidVoxelMorphNormalizer::RigidVoxelMorphNormalizer(ConfigurationPtr config) 
    : config_(config) {
    initializeModel();
}

void RigidVoxelMorphNormalizer::initializeModel() {
    std::string rigidModelPath = config_->getModelPath("rigid");
    std::string voxelMorphPath = config_->getModelPath("affine_voxelmorph");
    std::string templatePath = config_->getTemplatePath("padded");
    
    registrationPipeline_ = std::make_unique<RegistrationPipeline>(rigidModelPath, voxelMorphPath);
    paddedTemplate_ = Common::nifti::loadImage(templatePath);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalize(ImageType::Pointer inputImage) {
    // Standard spatial normalization pipeline
    ImageType::Pointer rigidImage = performRigidAlignment(inputImage);
    saveDebugImage(rigidImage, "rigid");
    return performVoxelMorphWarping(rigidImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeRigidOnly(ImageType::Pointer inputImage) {
    ImageType::Pointer rigidImage = performRigidAlignment(inputImage);
    saveDebugImage(rigidImage, "rigid");
    return rigidImage;
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeIterativeRigidOnly(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    return performIterativeRigidAlignment(inputImage, maxIter, threshold);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeIterative(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    ImageType::Pointer currentImage = performIterativeRigidAlignment(inputImage, maxIter, threshold);
    return performVoxelMorphWarping(currentImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::normalizeManualFOV(ImageType::Pointer inputImage) {
    // Skip rigid registration, perform nonlinear registration directly
    return performVoxelMorphWarping(inputImage);
}

RigidVoxelMorphNormalizer::NormalizationResult RigidVoxelMorphNormalizer::normalizeWithIntermediateResults(ImageType::Pointer inputImage) {
    NormalizationResult result;
    
    // Perform rigid alignment
    result.rigidAlignedImage = performRigidAlignment(inputImage);
    saveDebugImage(result.rigidAlignedImage, "rigid");
    
    // Perform VoxelMorph warping
    result.spatiallyNormalizedImage = performVoxelMorphWarping(result.rigidAlignedImage);
    
    return result;
}

RigidVoxelMorphNormalizer::NormalizationResult RigidVoxelMorphNormalizer::normalizeIterativeWithIntermediateResults(ImageType::Pointer inputImage, int maxIter, float threshold) {
    NormalizationResult result;
    
    result.rigidAlignedImage = performIterativeRigidAlignment(inputImage, maxIter, threshold);
    result.spatiallyNormalizedImage = performVoxelMorphWarping(result.rigidAlignedImage);
    
    return result;
}

RigidVoxelMorphNormalizer::RigidAlignmentEstimate RigidVoxelMorphNormalizer::estimateRigidAlignment(
    ImageType::Pointer inputImage, bool resampleFirst) {
    ImageType::Pointer processedImage = inputImage;
    
    if (resampleFirst) {
        processedImage = Common::image::resampleToMatch(paddedTemplate_, processedImage);
    }
    
    processedImage = registrationPipeline_->preprocess(processedImage);
    saveDebugImage(processedImage, "rigid_preprocessed");
    
    // Extract image data
    std::vector<float> imageData;
    Common::image::extractImageData(processedImage, imageData);
    
    // Execute prediction
    auto orientation = registrationPipeline_->predict(imageData, {1, 1, 64, 64, 64});

    return RigidAlignmentEstimate{processedImage, orientation};
}

void RigidVoxelMorphNormalizer::applyRigidAlignmentEstimate(
    ImageType::Pointer targetImage, const RigidAlignmentEstimate& estimate) {
    if (!targetImage) {
        return;
    }
    
    // Get new origin and direction
    auto newOriginAndDirection = registrationPipeline_->getNewOriginAndDirection(
        estimate.preprocessedImage, targetImage,
        estimate.orientation.at("ac"),
        estimate.orientation.at("pa"),
        estimate.orientation.at("is"));
    
    ImageType::PointType newOrigin = std::get<0>(newOriginAndDirection);
    ImageType::DirectionType newDirection = std::get<1>(newOriginAndDirection);
    
    targetImage->SetDirection(newDirection);
    targetImage->SetOrigin(newOrigin);
}

ImageType::Pointer RigidVoxelMorphNormalizer::performRigidAlignment(ImageType::Pointer inputImage, bool resampleFirst) {
    auto estimate = estimateRigidAlignment(inputImage, resampleFirst);
    applyRigidAlignmentEstimate(inputImage, estimate);
    
    return inputImage;
}

ImageType::Pointer RigidVoxelMorphNormalizer::performIterativeRigidAlignment(
    ImageType::Pointer inputImage, int maxIter, float threshold) {
    ImageType::Pointer currentImage = performRigidAlignment(inputImage, false);
    saveDebugImage(currentImage, "rigid0");
    ImageType::PointType lastOrigin = currentImage->GetOrigin();

    for (int i = 0; i < maxIter; ++i) {
        currentImage = performRigidAlignment(currentImage, true);
        saveDebugImage(currentImage, "rigid" + std::to_string(i + 1));

        float originShift = 0;
        for (int j = 0; j < 3; ++j) {
            originShift += std::pow(currentImage->GetOrigin()[j] - lastOrigin[j], 2);
        }
        originShift = std::sqrt(originShift);

        if (originShift < threshold) {
            break;
        }
        lastOrigin = currentImage->GetOrigin();
    }

    return currentImage;
}

ImageType::Pointer RigidVoxelMorphNormalizer::performVoxelMorphWarping(ImageType::Pointer rigidImage) {
    // Resample to template space
    ImageType::Pointer paddedImage = Common::image::resampleToMatch(paddedTemplate_, rigidImage);
    
    // Preprocessing
    paddedImage = registrationPipeline_->preprocessVoxelMorph(paddedImage);
    saveDebugImage(paddedImage, "elastic_preprocessed");
    
    // Prepare data
    std::vector<float> paddedImageData, paddedTemplateData, paddedOriginalData;
    Common::image::extractImageData(paddedImage, paddedImageData);
    Common::image::extractImageData(paddedTemplate_, paddedTemplateData);
    
    // Load original resampled image
    ImageType::Pointer paddedOriginalImage = Common::image::resampleToMatch(paddedTemplate_, rigidImage);
    Common::image::extractImageData(paddedOriginalImage, paddedOriginalData);
    
    // Execute VoxelMorph prediction
    auto warpedImageData = registrationPipeline_->predictVoxelMorph(
        paddedOriginalData, paddedImageData, paddedTemplateData);
    
    // Create output image
    ImageType::Pointer warpedImage = Common::image::createImageFromVector(
        warpedImageData["warped"], paddedImage->GetLargestPossibleRegion().GetSize());
    
    warpedImage->SetDirection(paddedTemplate_->GetDirection());
    warpedImage->SetOrigin(paddedTemplate_->GetOrigin());
    warpedImage->SetSpacing(paddedTemplate_->GetSpacing());
    
    // Crop to MNI space
    return cropMNI(warpedImage);
}

ImageType::Pointer RigidVoxelMorphNormalizer::cropMNI(ImageType::Pointer image) {
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

std::string RigidVoxelMorphNormalizer::getName() const {
    return "RigidVoxelMorph";
}

bool RigidVoxelMorphNormalizer::isSupported(const std::string& modality) const {
    // Support all modalities
    return true;
}

void RigidVoxelMorphNormalizer::setDebugMode(bool enable, const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
}

void RigidVoxelMorphNormalizer::saveDebugImage(ImageType::Pointer image, const std::string& suffix) {
    if (!debugMode_ || debugBasePath_.empty()) return;
    std::string filename = debugBasePath_ + "_" + suffix + ".nii";
    Common::nifti::saveImage(image, filename);
}
