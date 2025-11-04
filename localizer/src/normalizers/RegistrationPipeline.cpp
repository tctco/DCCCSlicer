#include "RegistrationPipeline.h"

RegistrationPipeline::RegistrationPipeline(const std::string& rigidModelPath, 
                                          const std::string& nonlinearModelPath) {
    rigidEngine_ = std::make_unique<RigidRegistrationEngine>(rigidModelPath);
    nonlinearEngine_ = std::make_unique<NonlinearRegistrationEngine>(nonlinearModelPath);
}

ImageType::Pointer RegistrationPipeline::preprocess(ImageType::Pointer image) {
    return ImagePreprocessor::preprocessForRigid(image);
}

ImageType::Pointer RegistrationPipeline::preprocessVoxelMorph(ImageType::Pointer image) {
    return ImagePreprocessor::preprocessForVoxelMorph(image);
}

std::unordered_map<std::string, std::vector<float>> RegistrationPipeline::predict(
    std::vector<float> inputTensor, const std::vector<int64_t> inputShape) {
    return rigidEngine_->predict(inputTensor, inputShape);
}

std::unordered_map<std::string, std::vector<float>> RegistrationPipeline::predictVoxelMorph(
    std::vector<float> originalImg, std::vector<float> movingImg,
    std::vector<float> templateImg) {
    return nonlinearEngine_->predict(originalImg, movingImg, templateImg);
}

std::tuple<ImageType::PointType, ImageType::DirectionType>
RegistrationPipeline::getNewOriginAndDirection(
    ImageType::Pointer preprocessedImage, ImageType::Pointer originalImage,
    std::vector<float> ac, std::vector<float> pa, std::vector<float> is) {
    return rigidEngine_->getNewOriginAndDirection(preprocessedImage, originalImage, ac, pa, is);
}
