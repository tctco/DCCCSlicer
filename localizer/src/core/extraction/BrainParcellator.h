#pragma once

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"

#include <cstddef>
#include <string>
#include <vector>

class BrainParcellator {
public:
    struct Region {
        std::string key;
        Common::nifti::BinaryImageType::Pointer mask;
        std::size_t voxelCount = 0;
        double volumeMl = 0.0;
    };

    struct Result {
        std::vector<Region> regions;
        double voxelVolumeMl = 0.0;
    };

    explicit BrainParcellator(ConfigurationPtr config);

    Result parcellate(ImageType::Pointer inputImage,
                      ImageType::Pointer templateAparcAseg,
                      ImageType::Pointer templateIntracranialMask,
                      float threshold = 0.5f);
    void setDebugMode(bool enable, const std::string& basePath = "");

private:
    ConfigurationPtr config_;
    bool debugMode_ = false;
    std::string debugBasePath_;
};
