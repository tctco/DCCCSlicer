#pragma once
#include "onnxruntime_cxx_api.h"
#include <cstddef>
#include <string>
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
        const std::vector<float>& templateImg,
        const std::vector<float>* templateImage = nullptr,
        std::size_t templateImageChannels = 1);

    bool supportsInverseWarp() const;

private:
    Ort::Env env_;
    Ort::Session* session_;
    bool hasTemplateImageInput_ = false;
    bool hasInverseWarpOutput_ = false;
};
