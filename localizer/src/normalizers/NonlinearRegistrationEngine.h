#pragma once
#include "../utils/common.h"
#include "onnxruntime_cxx_api.h"
#include <unordered_map>
#include <vector>

/**
 * @brief Nonlinear registration engine using VoxelMorph
 */
class NonlinearRegistrationEngine {
public:
    explicit NonlinearRegistrationEngine(const std::string& modelPath);
    ~NonlinearRegistrationEngine();
    
    /**
     * @brief Predict nonlinear transformation using VoxelMorph
     */
    std::unordered_map<std::string, std::vector<float>> predict(
        const std::vector<float>& originalImg, 
        const std::vector<float>& movingImg,
        const std::vector<float>& templateImg);

private:
    Ort::Env env_;
    Ort::Session* session_;
};
