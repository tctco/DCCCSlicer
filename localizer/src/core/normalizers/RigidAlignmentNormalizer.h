#pragma once

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"
#include "RigidRegistrationEngine.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class RigidAlignmentNormalizer {
public:
    explicit RigidAlignmentNormalizer(ConfigurationPtr config);

    ImageType::Pointer align(ImageType::Pointer inputImage);
    ImageType::Pointer alignIterative(ImageType::Pointer inputImage,
                                      int maxIter = 5,
                                      float threshold = 2.0f);

    void setDebugMode(bool enable, const std::string& basePath = "");

private:
    struct AlignmentEstimate {
        ImageType::Pointer preprocessedImage;
        std::unordered_map<std::string, std::vector<float>> orientation;
    };

    ConfigurationPtr config_;
    std::unique_ptr<RigidRegistrationEngine> rigidEngine_;
    ImageType::Pointer paddedTemplate_;
    bool debugMode_ = false;
    std::string debugBasePath_;

    AlignmentEstimate estimate(ImageType::Pointer inputImage, bool resampleFirst = false);
    void apply(ImageType::Pointer targetImage, const AlignmentEstimate& estimate);
    ImageType::Pointer performAlignment(ImageType::Pointer inputImage, bool resampleFirst = false);
    void saveDebugImage(ImageType::Pointer image, const std::string& suffix);
};
