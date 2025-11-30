#pragma once

#include <string>

#include "NiftiTypes.h"

namespace refactorCommon::nifti {

void saveImage(ImageType::Pointer image, const std::string& filename);
ImageType::Pointer loadImage(const std::string& filename);

}  // namespace refactorCommon::nifti


