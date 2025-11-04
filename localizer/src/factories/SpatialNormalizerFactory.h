#pragma once
#include "../interfaces/ISpatialNormalizer.h"
#include "../interfaces/IConfiguration.h"
#include <string>
#include <memory>
#include <vector>

/**
 * @brief Spatial normalizer factory
 */
class SpatialNormalizerFactory {
public:
    enum class NormalizerType {
        RIGID_VOXELMORPH,
        // Future types can be added here
        // AFFINE_ONLY,
        // DEFORMABLE_ONLY,
        // CUSTOM_MODEL
    };
    
    static SpatialNormalizerPtr create(NormalizerType type, ConfigurationPtr config);
    static SpatialNormalizerPtr createFromString(const std::string& typeName, ConfigurationPtr config);
    static std::vector<std::string> getAvailableTypes();
    
private:
    static NormalizerType stringToType(const std::string& typeName);
};

