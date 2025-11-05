#include "Decoupler.h"
#include "../utils/onnx_path_utils.h"

void DecoupledResult::SaveResults(const std::string& fpath) {
  Common::SaveImage(strippedImage,
                    Common::addSuffixToFilePath(fpath, "_stripped_image"));
  Common::SaveImage(strippedComponent, Common::addSuffixToFilePath(
                                           fpath, "_stripped_component"));
  Common::SaveImage(ADprobMap,
                    Common::addSuffixToFilePath(fpath, "_AD_prob_map"));
}

void DecoupledResult::printResult() const {
  std::cout << "AI can make mistakes, please double check the results."
            << std::endl;
  std::cout << "AD probability: " << this->ADprob * 100 << "%" << std::endl;
  if (!ADADTracerValues.empty()) {
    for (const auto& kv : ADADTracerValues) {
      std::cout << kv.first << ": " << kv.second << std::endl;
    }
  } else {
    std::cout << "ADAD score: " << this->ADADscore << std::endl;
  }
}

Decoupler::Decoupler(const std::string& modelPath)
    : env(ORT_LOGGING_LEVEL_WARNING, "Decouple"), sessions() {
  Ort::SessionOptions sessionOptions;
  sessionOptions.SetIntraOpNumThreads(1);
  auto ortModelPath = OrtUtils::MakeOrtPath(modelPath);
  try {
    this->sessions.push_back(
        new Ort::Session(this->env, ortModelPath.c_str(), sessionOptions));
  } catch (const Ort::Exception& e) {
    std::cerr << "Error loading mode: " << e.what() << std::endl;
    throw std::runtime_error("Filed to load model");
  }
}
Decoupler::Decoupler(const std::vector<std::string>& modelPaths)
    : env(ORT_LOGGING_LEVEL_WARNING, "Decouple"), sessions() {
  Ort::SessionOptions sessionOptions;
  sessionOptions.SetIntraOpNumThreads(1);
  try {
    for (const auto& p : modelPaths) {
      auto ortModelPath = OrtUtils::MakeOrtPath(p);
      this->sessions.push_back(
          new Ort::Session(this->env, ortModelPath.c_str(), sessionOptions));
    }
  } catch (const Ort::Exception& e) {
    std::cerr << "Error loading mode: " << e.what() << std::endl;
    throw std::runtime_error("Filed to load model");
  }
}
Decoupler::~Decoupler() {
  for (auto* s : this->sessions) {
    delete s;
  }
  this->sessions.clear();
}

std::unordered_map<std::string, std::vector<float>> Decoupler::_predict_one(
    Ort::Session* session, std::vector<float>& inputTensor) {
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::MemoryInfo memoryInfo =
      Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  size_t inputTensorSize = inputTensor.size() * sizeof(float);
  Ort::Value inputTensorValue = Ort::Value::CreateTensor<float>(
      memoryInfo, inputTensor.data(), inputTensorSize, this->INPUT_SHAPE.data(),
      this->INPUT_SHAPE.size());
  const char* inputName = "PET";
  std::vector<const char*> outputNames = {
      "stripped_AD_images_cal", "stripped_component_cal",
      "AD_prob_map_cal",        "AD_prob",
      "ADAD_scores_cal",        };
  std::vector<size_t> outputSizes = {
      INPUT_TENSOR_SIZE, INPUT_TENSOR_SIZE, INPUT_TENSOR_SIZE, 1, 1};
  size_t numOutputs = outputNames.size();

  // Run inference

  std::vector<Ort::Value> outputTensorValues =
      session->Run(Ort::RunOptions{nullptr}, &inputName, &inputTensorValue, 1,
                   outputNames.data(), numOutputs);
  std::unordered_map<std::string, std::vector<float>> result;
  for (size_t i = 0; i < outputNames.size(); ++i) {
    std::string outputName(outputNames[i]);

    auto& outputTensor = outputTensorValues[i];
    float* outputData = outputTensor.GetTensorMutableData<float>();
    size_t outputSize = outputSizes[i];

    result[outputName].assign(outputData, outputData + outputSize);
  }
  return result;
}

DecoupledResult Decoupler::predict(ImageType::Pointer inputImage) {
  // Convert input image to vector
  std::vector<float> inputTensor;
  Common::ExtractImageData(inputImage, inputTensor);

  // Run inference (support ensemble) and aggregate
  std::unordered_map<std::string, std::vector<float>> aggregated;
  size_t modelCount = this->sessions.size();
  for (auto* s : this->sessions) {
    auto one = this->_predict_one(s, inputTensor);
    for (auto& kv : one) {
      const std::string& name = kv.first;
      const std::vector<float>& data = kv.second;
      auto& acc = aggregated[name];
      if (acc.empty()) acc.assign(data.size(), 0.0f);
      for (size_t i = 0; i < data.size(); ++i) acc[i] += data[i];
    }
  }
  // Average
  for (auto& kv : aggregated) {
    auto& v = kv.second;
    for (auto& x : v) x /= static_cast<float>(modelCount);
  }

  // Convert output tensors to images
  DecoupledResult decoupledResult;
  auto size = inputImage->GetLargestPossibleRegion().GetSize();
  for (auto& [name, imgData] : aggregated) {
    if (name == "stripped_AD_images_cal") {
      ImageType::Pointer image = Common::CreateImageFromVector(imgData, size);
      image->SetOrigin(inputImage->GetOrigin());
      image->SetSpacing(inputImage->GetSpacing());
      image->SetDirection(inputImage->GetDirection());
      decoupledResult.strippedImage = image;
    } else if (name == "stripped_component_cal") {
      ImageType::Pointer image =
          Common::CreateImageFromVector(imgData, {160, 160, 96});
      image->SetOrigin(inputImage->GetOrigin());
      image->SetSpacing(inputImage->GetSpacing());
      image->SetDirection(inputImage->GetDirection());
      decoupledResult.strippedComponent = image;
    } else if (name == "AD_prob_map_cal") {
      ImageType::Pointer image =
          Common::CreateImageFromVector(imgData, {160, 160, 96});
      decoupledResult.ADprobMap = image;
      image->SetOrigin(inputImage->GetOrigin());
      image->SetSpacing(inputImage->GetSpacing());
      image->SetDirection(inputImage->GetDirection());
    } else if (name == "AD_prob") {
      decoupledResult.ADprob = imgData[0];
    } else if (name == "ADAD_scores_cal") {
      decoupledResult.ADADscore = imgData[0];
    } else {
      std::cerr << "Unknown output tensor: " << name << std::endl;
    }
  }

  return decoupledResult;
}
