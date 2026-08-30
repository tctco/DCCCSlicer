#pragma once

#include <vector>

#include "NiftiTypes.h"

namespace Common::image {

using ImageType = nifti::ImageType;

void divideVoxelsByValue(ImageType::Pointer image, float divisor);
void normalizeFdgIterativeGlobalMean(ImageType::Pointer image);
double calculateMeanInMask(ImageType::Pointer image,
                           ImageType::Pointer mask);
ImageType::Pointer resampleToMatch(ImageType::Pointer referenceImage,
                                   ImageType::Pointer inputImage);
ImageType::Pointer createImageFromVector(const std::vector<float>& imageData,
                                         ImageType::SizeType size);
void extractImageData(ImageType::Pointer image, std::vector<float>& imageData);

}  // namespace Common::image
