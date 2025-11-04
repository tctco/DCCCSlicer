#pragma once
#include "../utils/common.h"
#include "onnxruntime_cxx_api.h"
#include <unordered_map>
#include <vector>
#include <tuple>

/**
 * @brief Rigid registration engine using deep learning
 */
class RigidRegistrationEngine {
public:
    explicit RigidRegistrationEngine(const std::string& modelPath);
    ~RigidRegistrationEngine();
    
    /**
     * @brief Predict rigid transformation parameters
     */
    std::unordered_map<std::string, std::vector<float>> predict(
        const std::vector<float>& inputTensor, 
        const std::vector<int64_t>& inputShape);
    
    /**
     * @brief Calculate new origin and direction from landmark predictions
     */
    std::tuple<ImageType::PointType, ImageType::DirectionType> 
    getNewOriginAndDirection(
        ImageType::Pointer preprocessedImage, 
        ImageType::Pointer originalImage,
        const std::vector<float>& ac, 
        const std::vector<float>& pa,
        const std::vector<float>& is);

private:
    Ort::Env env_;
    Ort::Session* session_;
};
