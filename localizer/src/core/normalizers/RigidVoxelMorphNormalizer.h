#pragma once
#include "../interfaces/ISpatialNormalizer.h"
#include "../interfaces/IConfiguration.h"
#include "RigidAlignmentNormalizer.h"
#include "VoxelMorphNormalizer.h"
#include <memory>

/**
 * @brief Spatial normalizer based on rigid registration and VoxelMorph
 */
class RigidVoxelMorphNormalizer : public ISpatialNormalizer {
public:
    explicit RigidVoxelMorphNormalizer(ConfigurationPtr config);
    virtual ~RigidVoxelMorphNormalizer() = default;
    
    ImageType::Pointer normalize(ImageType::Pointer inputImage) override;
    std::string getName() const override;
    bool isSupported(const std::string& modality) const override;
    
    // Extended functionality
    ImageType::Pointer normalizeRigidOnly(ImageType::Pointer inputImage);
    ImageType::Pointer normalizeIterativeRigidOnly(ImageType::Pointer inputImage, int maxIter = 5, float threshold = 2.0f);
    ImageType::Pointer normalizeIterative(ImageType::Pointer inputImage, int maxIter = 5, float threshold = 2.0f);
    ImageType::Pointer normalizeManualFOV(ImageType::Pointer inputImage);
    
    /**
     * @brief Normalize with both rigid and spatial results
     */
    struct NormalizationResult {
        ImageType::Pointer rigidAlignedImage;
        ImageType::Pointer spatiallyNormalizedImage;
    };
    NormalizationResult normalizeWithIntermediateResults(ImageType::Pointer inputImage);
    NormalizationResult normalizeIterativeWithIntermediateResults(ImageType::Pointer inputImage, int maxIter = 5, float threshold = 2.0f);
    
    // Debug functionality
    void setDebugMode(bool enable, const std::string& basePath = "");
    
private:
    ConfigurationPtr config_;
    std::unique_ptr<RigidAlignmentNormalizer> rigidNormalizer_;
    std::unique_ptr<VoxelMorphNormalizer> voxelMorphNormalizer_;
    
    // Debug parameters
    bool debugMode_ = false;
    std::string debugBasePath_ = "";

    RigidAlignmentNormalizer& rigidNormalizer();
    VoxelMorphNormalizer& voxelMorphNormalizer();
};
