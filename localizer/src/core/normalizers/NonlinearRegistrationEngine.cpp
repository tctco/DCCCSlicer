#include "NonlinearRegistrationEngine.h"
#include "../common/OnnxPath.h"

#include <iostream>
#include <stdexcept>

namespace {

constexpr std::size_t kVoxelCount = 96 * 128 * 96;

bool sessionHasInput(Ort::Session& session, const char* expectedName) {
    Ort::AllocatorWithDefaultOptions allocator;
    for (std::size_t i = 0; i < session.GetInputCount(); ++i) {
        auto name = session.GetInputNameAllocated(i, allocator);
        if (name && std::string(name.get()) == expectedName) {
            return true;
        }
    }
    return false;
}

bool sessionHasOutput(Ort::Session& session, const char* expectedName) {
    Ort::AllocatorWithDefaultOptions allocator;
    for (std::size_t i = 0; i < session.GetOutputCount(); ++i) {
        auto name = session.GetOutputNameAllocated(i, allocator);
        if (name && std::string(name.get()) == expectedName) {
            return true;
        }
    }
    return false;
}

void requireTensorSize(const std::vector<float>& values, const char* name) {
    if (values.size() != kVoxelCount) {
        throw std::invalid_argument(
            std::string("Nonlinear registration input '") + name +
            "' must contain exactly " + std::to_string(kVoxelCount) + " voxels");
    }
}

void requireTemplateTensorSize(const std::vector<float>& values,
                               std::size_t channels) {
    if (channels == 0 || values.size() != channels * kVoxelCount) {
        throw std::invalid_argument(
            "Nonlinear registration input 'template_image' must contain " +
            std::to_string(channels) + " channel(s) of " +
            std::to_string(kVoxelCount) + " voxels");
    }
}

}  // namespace

NonlinearRegistrationEngine::NonlinearRegistrationEngine(const std::string& modelPath)
    : env_(ORT_LOGGING_LEVEL_WARNING, "NonlinearRegistration"), session_(nullptr) {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetInterOpNumThreads(1);
    sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    // Even BASIC graph optimization creates multi-gigabyte temporary memory plans
    // for chained 3D GridSample nodes in this model. Keep this disabled: it is the
    // difference between roughly 1 GB and more than 9 GB for the inverse export.
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    // The default arena retains very large temporary buffers from this 3D graph.
    // Releasing tensors as soon as their nodes finish keeps this model usable on
    // memory-constrained laptops, at the cost of some allocator overhead.
    sessionOptions.DisableMemPattern();
    sessionOptions.DisableCpuMemArena();
    auto ortModelPath = Common::onnx::makeOrtPath(modelPath);

    try {
        session_ = new Ort::Session(env_, ortModelPath.c_str(), sessionOptions);
        hasTemplateImageInput_ = sessionHasInput(*session_, "template_image");
        hasInverseWarpOutput_ = sessionHasOutput(*session_, "inverse_warped");
    } catch (const Ort::Exception& e) {
        std::cerr << "Error loading nonlinear registration model: " << e.what() << std::endl;
        throw std::runtime_error("Failed to load nonlinear registration model.");
    }
}

NonlinearRegistrationEngine::~NonlinearRegistrationEngine() {
    if (session_) delete session_;
}

std::unordered_map<std::string, std::vector<float>> NonlinearRegistrationEngine::predict(
    const std::vector<float>& originalImg,
    const std::vector<float>& movingImg,
    const std::vector<float>& templateImg,
    const std::vector<float>* templateImage,
    std::size_t templateImageChannels) {
    requireTensorSize(originalImg, "input_raw");
    requireTensorSize(movingImg, "input");
    requireTensorSize(templateImg, "template");
    if (templateImage) {
        requireTemplateTensorSize(*templateImage, templateImageChannels);
        if (!supportsInverseWarp()) {
            throw std::runtime_error(
                "The configured affine VoxelMorph model does not support inverse warping");
        }
    }

    std::vector<const char*> inputNames = {"input", "template", "input_raw"};
    const std::vector<int64_t> inputShape = {1, 1, 96, 128, 96};
    Ort::MemoryInfo memoryInfo =
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);

    std::vector<Ort::Value> inputTensors;
    inputTensors.reserve(hasTemplateImageInput_ ? 4 : 3);
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(movingImg.data()), movingImg.size(), 
        inputShape.data(), inputShape.size()));
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(templateImg.data()), templateImg.size(), 
        inputShape.data(), inputShape.size()));
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(originalImg.data()), originalImg.size(), 
        inputShape.data(), inputShape.size()));

    // The inverse-capable export requires all four inputs even when callers only
    // request the forward-warped PET image. In that case the template itself is a
    // harmless placeholder because ONNX Runtime prunes the unrequested inverse branch.
    if (hasTemplateImageInput_) {
        const std::vector<float>& inverseInput = templateImage ? *templateImage : templateImg;
        const std::size_t inverseChannels = templateImage ? templateImageChannels : 1;
        const std::vector<int64_t> inverseInputShape = {
            1, static_cast<int64_t>(inverseChannels), 96, 128, 96};
        inputNames.push_back("template_image");
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float*>(inverseInput.data()), inverseInput.size(),
            inverseInputShape.data(), inverseInputShape.size()));
    }

    std::vector<const char*> outputNames = {
        templateImage ? "inverse_warped" : "warped"};

    auto outputTensors = session_->Run(
        Ort::RunOptions{nullptr}, inputNames.data(), inputTensors.data(), inputTensors.size(),
        outputNames.data(), outputNames.size());

    std::unordered_map<std::string, std::vector<float>> output;
    for (std::size_t i = 0; i < outputTensors.size(); ++i) {
        const Ort::TensorTypeAndShapeInfo outputInfo =
            outputTensors[i].GetTensorTypeAndShapeInfo();
        const float* outputData = outputTensors[i].GetTensorData<float>();
        std::vector<float> outputVector(outputData,
                                        outputData + outputInfo.GetElementCount());
        output[outputNames[i]] = outputVector;
    }
    return output;
}

bool NonlinearRegistrationEngine::supportsInverseWarp() const {
    return hasTemplateImageInput_ && hasInverseWarpOutput_;
}
