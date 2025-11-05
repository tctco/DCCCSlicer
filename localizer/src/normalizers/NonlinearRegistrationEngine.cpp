#include "NonlinearRegistrationEngine.h"
#include <iostream>

NonlinearRegistrationEngine::NonlinearRegistrationEngine(const std::string& modelPath)
    : env_(ORT_LOGGING_LEVEL_WARNING, "NonlinearRegistration"), session_(nullptr) {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    const ORTCHAR_T* ortModelPath = nullptr;
#ifdef _WIN32
    std::wstring w_modelPath(modelPath.begin(), modelPath.end());
    ortModelPath = w_modelPath.c_str();
#else
    ortModelPath = modelPath.c_str();
#endif

    try {
        session_ = new Ort::Session(env_, ortModelPath, sessionOptions);
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
    const std::vector<float>& templateImg) {
    
    Ort::AllocatorWithDefaultOptions allocator;
    const char* inputNames[] = {"input", "template", "input_raw"};
    std::vector<int64_t> inputShape = {1, 1, 79 + 17, 95 + 33, 79 + 17};
    
    Ort::MemoryInfo memoryInfo =
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);
    
    std::vector<Ort::Value> inputTensors;
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(movingImg.data()), movingImg.size(), 
        inputShape.data(), inputShape.size()));
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(templateImg.data()), templateImg.size(), 
        inputShape.data(), inputShape.size()));
    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, const_cast<float*>(originalImg.data()), originalImg.size(), 
        inputShape.data(), inputShape.size()));
    
    const char* outputNames[] = {"warped"};
    size_t numOutputs = 1;

    std::vector<Ort::Value> outputTensors;
    std::vector<std::vector<float>> outputBuffers(
        numOutputs, std::vector<float>(1 * 3 * 96 * 128 * 96));
    for (size_t i = 0; i < numOutputs; ++i) {
        Ort::Value outputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, outputBuffers[i].data(), outputBuffers[i].size(),
            std::vector<int64_t>{1, 1, 96, 128, 96}.data(), 5);
        outputTensors.push_back(std::move(outputTensor));
    }

    session_->Run(Ort::RunOptions{nullptr}, inputNames,
                  inputTensors.data(), 3, outputNames,
                  outputTensors.data(), numOutputs);

    std::unordered_map<std::string, std::vector<float>> output;
    for (size_t i = 0; i < outputTensors.size(); i++) {
        Ort::TensorTypeAndShapeInfo outputInfo =
            outputTensors[i].GetTensorTypeAndShapeInfo();
        float* outputData = outputTensors[i].GetTensorMutableData<float>();
        std::vector<float> outputVector(outputData,
                                        outputData + outputInfo.GetElementCount());
        output[outputNames[i]] = outputVector;
    }
    return output;
}
