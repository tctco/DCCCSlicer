#pragma once

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"

#include <cstddef>
#include <string>

class BrainExtractor {
public:
    struct Result {
        ImageType::Pointer strippedImage;
        Common::nifti::BinaryImageType::Pointer brainMask;
        std::size_t brainVoxelCount = 0;
    };

    explicit BrainExtractor(ConfigurationPtr config);

    Result extract(ImageType::Pointer inputImage,
                   ImageType::Pointer templateBrainMask,
                   float threshold = 0.5f);
    void setDebugMode(bool enable, const std::string& basePath = "");

private:
    ConfigurationPtr config_;
    bool debugMode_ = false;
    std::string debugBasePath_;
};
