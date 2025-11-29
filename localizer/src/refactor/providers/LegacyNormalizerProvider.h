#pragma once
#include "../../factories/SpatialNormalizerFactory.h"
#include "../../normalizers/RigidVoxelMorphNormalizer.h"

namespace RefactorPipeline {

class LegacyNormalizerProvider {
public:
    explicit LegacyNormalizerProvider(ConfigurationPtr config);

    std::shared_ptr<RigidVoxelMorphNormalizer> createRigidVoxelMorphNormalizer();

private:
    ConfigurationPtr config_;
};

} // namespace RefactorPipeline

