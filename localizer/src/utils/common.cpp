#include "common.h"

namespace Common {

std::string getExecutablePath() {
  return refactorCommon::path::executableDirectory();
}

void DivideVoxelsByValue(ImageType::Pointer image, float divisor) {
  refactorCommon::image::divideVoxelsByValue(image, divisor);
}

double CalculateMeanInMask(ImageType::Pointer image, ImageType::Pointer mask) {
  return refactorCommon::image::calculateMeanInMask(image, mask);
}

ImageType::Pointer CreateImageFromVector(const std::vector<float>& imageData,
                                         ImageType::SizeType size) {
  return refactorCommon::image::createImageFromVector(imageData, size);
}

ImageType::Pointer ResampleToMatch(ImageType::Pointer referenceImage,
                                   ImageType::Pointer inputImage) {
  return refactorCommon::image::resampleToMatch(referenceImage, inputImage);
}

void SaveImage(ImageType::Pointer image, const std::string& filename) {
  refactorCommon::nifti::saveImage(image, filename);
}

ImageType::Pointer LoadNii(const std::string& filename) {
  return refactorCommon::nifti::loadImage(filename);
}

void ExtractImageData(ImageType::Pointer image, std::vector<float>& imageData) {
  refactorCommon::image::extractImageData(image, imageData);
}

std::string addSuffixToFilePath(const std::string& filePath,
                                const std::string& suffix) {
  return refactorCommon::path::addSuffix(filePath, suffix);
}

void debugLog(const std::string& message) {
  refactorCommon::log::debug(message);
}

std::string toLower(const std::string& str) {
  return refactorCommon::path::toLower(str);
}
}  // namespace Common
