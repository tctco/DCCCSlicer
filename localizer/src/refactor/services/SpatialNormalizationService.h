#pragma once
#include "ISpatialNormalizationService.h"
#include "../providers/LegacyNormalizerProvider.h"
#include <memory>

namespace RefactorPipeline {

class SpatialNormalizationService : public ISpatialNormalizationService {
public:
    SpatialNormalizationService(ConfigurationPtr config,
                                std::shared_ptr<LegacyNormalizerProvider> provider);

    SpatialNormalizationOutput normalize(const SpatialNormalizationRequest& request) override;

private:
    ConfigurationPtr config_;
    std::shared_ptr<LegacyNormalizerProvider> provider_;

    ImageType::Pointer loadInput(const std::string& inputPath) const;
};

} // namespace RefactorPipeline

