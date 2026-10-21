#pragma once

#include <string>

#include "NiftiTypes.h"

namespace Common::nifti {

void saveImage(ImageType::Pointer image, const std::string& filename);
void saveBinaryImage(BinaryImageType::Pointer image, const std::string& filename);
void saveLabelImage(LabelImageType::Pointer image, const std::string& filename);
ImageType::Pointer loadImage(const std::string& filename, bool require3D = false);

}  // namespace Common::nifti

