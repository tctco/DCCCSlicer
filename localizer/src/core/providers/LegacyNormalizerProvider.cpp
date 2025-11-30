#include "LegacyNormalizerProvider.h"
#include <stdexcept>
#include <utility>

namespace RefactorPipeline {

LegacyNormalizerProvider::LegacyNormalizerProvider(ConfigurationPtr config)
    : config_(std::move(config)) {
}

std::shared_ptr<RigidVoxelMorphNormalizer> LegacyNormalizerProvider::createRigidVoxelMorphNormalizer() {
    auto normalizer = SpatialNormalizerFactory::createFromString("rigid_voxelmorph", config_);
    auto rigidNormalizer = std::dynamic_pointer_cast<RigidVoxelMorphNormalizer>(normalizer);
    if (!rigidNormalizer) {
        throw std::runtime_error("Failed to create RigidVoxelMorphNormalizer from factory");
    }
    return rigidNormalizer;
}

} // namespace RefactorPipeline

