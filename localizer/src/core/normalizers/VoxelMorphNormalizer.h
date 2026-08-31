#pragma once

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"
#include "NonlinearRegistrationEngine.h"

#include <memory>
#include <string>

class VoxelMorphNormalizer {
public:
    explicit VoxelMorphNormalizer(
        ConfigurationPtr config,
        const std::string& modelName = "affine_voxelmorph");

    ImageType::Pointer normalize(ImageType::Pointer rigidImage);
    ImageType::Pointer inverseWarp(ImageType::Pointer rigidImage,
                                   ImageType::Pointer templateImage);
    void setDebugMode(bool enable, const std::string& basePath = "");

private:
    ConfigurationPtr config_;
    std::unique_ptr<NonlinearRegistrationEngine> nonlinearEngine_;
    ImageType::Pointer paddedTemplate_;
    bool debugMode_ = false;
    std::string debugBasePath_;

    ImageType::Pointer cropMNI(ImageType::Pointer image);
    void saveDebugImage(ImageType::Pointer image, const std::string& suffix);
};
