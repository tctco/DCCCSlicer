#pragma once

#include "../common/ImageTypes.h"
#include <array>
#include <itkImage.h>
#include <string>
#include <vector>

namespace Pipeline::Preprocessing {

using DynamicImageType = itk::Image<float, 4>;

struct PetMotionParameters {
    unsigned int frame = 0;
    double tx = 0.0;
    double ty = 0.0;
    double tz = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
};

struct PetMotionCorrectionResult {
    ImageType::Pointer averagedImage;
    DynamicImageType::Pointer correctedDynamicImage;
    std::vector<PetMotionParameters> motion;
};

class PetMotionCorrector {
public:
    static unsigned int inspectImageDimension(const std::string& inputPath);

    PetMotionCorrectionResult correct(const std::string& inputPath,
                                      bool retainCorrectedDynamic = false) const;

    static void saveAveragedImage(const PetMotionCorrectionResult& result,
                                  const std::string& outputPath);
    static void saveCorrectedDynamicImage(const PetMotionCorrectionResult& result,
                                          const std::string& outputPath);
    static void saveMotionParameters(const PetMotionCorrectionResult& result,
                                     const std::string& outputPath);
};

} // namespace Pipeline::Preprocessing
