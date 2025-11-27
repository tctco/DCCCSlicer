#pragma once
#include "../utils/common.h"
#include "../preprocessing/ImagePreprocessor.h"
#include "RigidRegistrationEngine.h"
#include "NonlinearRegistrationEngine.h"
#include <memory>
#include <string>

/**
 * @brief Complete registration pipeline combining rigid and nonlinear registration
 * Replaces the old "Rigid" class with better separation of concerns
 */
class RegistrationPipeline {
public:
    RegistrationPipeline(const std::string& rigidModelPath, const std::string& nonlinearModelPath);
    ~RegistrationPipeline() = default;
    
    // Main interface methods (compatible with old Rigid class interface)
    ImageType::Pointer preprocess(ImageType::Pointer image);
    ImageType::Pointer preprocessVoxelMorph(ImageType::Pointer image);
    
    std::unordered_map<std::string, std::vector<float>> predict(
        std::vector<float> inputTensor, const std::vector<int64_t> inputShape);
    
    std::unordered_map<std::string, std::vector<float>> predictVoxelMorph(
        std::vector<float> originalImg, std::vector<float> movingImg,
        std::vector<float> templateImg);
    
    std::tuple<ImageType::PointType, ImageType::DirectionType>
    getNewOriginAndDirection(
        ImageType::Pointer preprocessedImage, ImageType::Pointer originalImage,
        std::vector<float> ac, std::vector<float> pa, std::vector<float> is);

private:
    std::unique_ptr<RigidRegistrationEngine> rigidEngine_;
    std::unique_ptr<NonlinearRegistrationEngine> nonlinearEngine_;
};
