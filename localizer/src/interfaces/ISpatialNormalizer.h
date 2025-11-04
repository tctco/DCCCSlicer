#pragma once
#include "../utils/common.h"
#include <memory>
#include <string>

/**
 * @brief Spatial normalizer interface
 * Defines common interface for all spatial normalization algorithms
 */
class ISpatialNormalizer {
public:
    virtual ~ISpatialNormalizer() = default;
    
    /**
     * @brief Execute spatial normalization
     * @param inputImage Input image
     * @return Normalized image
     */
    virtual ImageType::Pointer normalize(ImageType::Pointer inputImage) = 0;
    
    /**
     * @brief Get normalizer name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Check if specified modality is supported
     */
    virtual bool isSupported(const std::string& modality) const = 0;
};

using SpatialNormalizerPtr = std::shared_ptr<ISpatialNormalizer>;

