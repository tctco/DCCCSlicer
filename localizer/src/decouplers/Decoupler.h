#pragma once

#include <iostream>
#include <unordered_map>
#include <map>

#include "../utils/common.h"
#include "onnxruntime_cxx_api.h"

class DecoupledResult {
 public:
  ImageType::Pointer strippedImage;
  ImageType::Pointer strippedComponent;
  ImageType::Pointer ADprobMap;
  float ADprob;
  float ADADscore;
  std::map<std::string, float> ADADTracerValues; // tracer -> converted ADAD

  void SaveResults(const std::string& fpath);
  void printResult() const;
};

class Decoupler {
 public:
  Decoupler(const std::string& modelPath);
  Decoupler(const std::vector<std::string>& modelPaths);
  ~Decoupler();
  std::unordered_map<std::string, std::vector<float>> _predict_one(
      Ort::Session* session, std::vector<float>& inputTensor);
  DecoupledResult predict(ImageType::Pointer inputImage);

 private:
  std::vector<int64_t> INPUT_SHAPE{1, 1, 160, 160, 96};
  std::vector<int64_t> IMAGE_SHAPE{160, 160, 96};
  std::size_t INPUT_TENSOR_SIZE = 160 * 160 * 96;
  Ort::Env env;
  std::vector<Ort::Session*> sessions;
};
