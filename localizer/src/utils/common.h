#pragma once
#ifndef COMMON_H
#define COMMON_H
#include "../core/common/Common.h"

#include <vector>
#include <string>

using ImageType = refactorCommon::ImageType;
using DDFType = refactorCommon::DDFType;
using BinaryImageType = refactorCommon::BinaryImageType;
using ReaderType = refactorCommon::ReaderType;

namespace Common {

void SaveImage(ImageType::Pointer image, const std::string& filename);
ImageType::Pointer LoadNii(const std::string& filename);
void DivideVoxelsByValue(ImageType::Pointer image, float divisor);
double CalculateMeanInMask(ImageType::Pointer image, ImageType::Pointer mask);
ImageType::Pointer ResampleToMatch(ImageType::Pointer referenceImage,
                                   ImageType::Pointer inputImage);
ImageType::Pointer CreateImageFromVector(const std::vector<float>& imageData,
                                         ImageType::SizeType size);
void ExtractImageData(ImageType::Pointer image, std::vector<float>& imageData);
std::string addSuffixToFilePath(const std::string& filePath,
                                const std::string& suffix);
void debugLog(const std::string& message);
std::string toLower(const std::string& str);
std::string getExecutablePath();
}  // namespace Common

#endif
