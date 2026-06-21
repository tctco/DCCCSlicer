#pragma once

#include "ImageTypes.h"
#include <string>

namespace Pipeline {

struct SpatialNormalizationOptions {
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool rigidOnly = false;
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool enableAdniPetCore = false;
    int maxIterations = 5;
    float convergenceThreshold = 2.0f;
};

struct SpatialNormalizationRequest {
    std::string inputPath;
    bool skip = false;
    SpatialNormalizationOptions options;
};

struct SpatialNormalizationOutput {
    ImageType::Pointer rigidAlignedImage;
    ImageType::Pointer spatiallyNormalizedImage;
};

struct FileSaveRequest {
    ImageType::Pointer spatiallyNormalizedImage;
    std::string outputPath;
};

} // namespace Pipeline
