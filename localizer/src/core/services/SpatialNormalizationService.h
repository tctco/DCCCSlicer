#pragma once
#include "ISpatialNormalizationService.h"
#include "../interfaces/IConfiguration.h"

namespace Pipeline {

class SpatialNormalizationService : public ISpatialNormalizationService {
public:
    explicit SpatialNormalizationService(ConfigurationPtr config);

    SpatialNormalizationOutput normalize(const SpatialNormalizationRequest& request) override;

private:
    ConfigurationPtr config_;

    ImageType::Pointer loadInput(const std::string& inputPath) const;
    ImageType::Pointer prepareAdniPetCoreImage(ImageType::Pointer rigidImage,
                                               ImageType::Pointer normalizedImage) const;
};

} // namespace Pipeline

