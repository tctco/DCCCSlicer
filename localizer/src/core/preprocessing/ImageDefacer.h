#pragma once

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"
#include "PetMotionCorrector.h"

namespace Pipeline::Preprocessing {

class ImageDefacer {
public:
  explicit ImageDefacer(ConfigurationPtr config);

  ImageType::Pointer
  createTemplateKeepMask(ImageType::Pointer templateBrainMask,
                         float marginMm = 10.0f) const;
  Common::nifti::BinaryImageType::Pointer
  createNativeKeepMask(ImageType::Pointer inputImage,
                       ImageType::Pointer templateBrainMask,
                       float threshold = 0.5f,
                       bool useIterativeRigid = false,
                       bool useManualFov = false,
                       float marginMm = 10.0f) const;

  static void applyMask(ImageType::Pointer image,
                        Common::nifti::BinaryImageType::Pointer keepMask);
  static void applyMask(DynamicImageType::Pointer image,
                        Common::nifti::BinaryImageType::Pointer keepMask);
  static void applyTemplateMask(ImageType::Pointer image,
                                ImageType::Pointer templateKeepMask,
                                float threshold = 0.5f);

private:
  ConfigurationPtr config_;
};

} // namespace Pipeline::Preprocessing
