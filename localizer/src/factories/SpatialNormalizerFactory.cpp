#include "SpatialNormalizerFactory.h"
#include "../normalizers/RigidVoxelMorphNormalizer.h"
#include <stdexcept>
#include <algorithm>
#include <string>

SpatialNormalizerPtr SpatialNormalizerFactory::create(NormalizerType type, ConfigurationPtr config) {
    switch (type) {
        case NormalizerType::RIGID_VOXELMORPH:
            return std::make_shared<RigidVoxelMorphNormalizer>(config);
        default:
            throw std::invalid_argument("Unknown spatial normalizer type");
    }
}

SpatialNormalizerPtr SpatialNormalizerFactory::createFromString(const std::string& typeName, ConfigurationPtr config) {
    return create(stringToType(typeName), config);
}

std::vector<std::string> SpatialNormalizerFactory::getAvailableTypes() {
    return {"rigid_voxelmorph"};
}

SpatialNormalizerFactory::NormalizerType SpatialNormalizerFactory::stringToType(const std::string& typeName) {
    std::string lowerName = typeName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName == "rigid_voxelmorph" || lowerName == "default") {
        return NormalizerType::RIGID_VOXELMORPH;
    }
    
    throw std::invalid_argument("Unsupported spatial normalizer type: " + typeName);
}

