#pragma once
#include "../interfaces/ISpatialNormalizer.h"
#include "../interfaces/IConfiguration.h"
#include "RegistrationPipeline.h"  // Use new registration pipeline
#include <unordered_map>

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
    std::unique_ptr<RegistrationPipeline> registrationPipeline_;
    ImageType::Pointer paddedTemplate_;
    
    // Debug parameters
    bool debugMode_ = false;
    std::string debugBasePath_ = "";

    struct RigidAlignmentEstimate {
        ImageType::Pointer preprocessedImage;
        std::unordered_map<std::string, std::vector<float>> orientation;
    };
    
    void initializeModel();
    ImageType::Pointer cropMNI(ImageType::Pointer image);
    RigidAlignmentEstimate estimateRigidAlignment(ImageType::Pointer inputImage, bool resampleFirst = false);
    void applyRigidAlignmentEstimate(ImageType::Pointer targetImage, const RigidAlignmentEstimate& estimate);
    ImageType::Pointer performRigidAlignment(ImageType::Pointer inputImage, bool resampleFirst = false);
    ImageType::Pointer performIterativeRigidAlignment(ImageType::Pointer inputImage, int maxIter, float threshold);
    ImageType::Pointer performVoxelMorphWarping(ImageType::Pointer rigidImage);
    void saveDebugImage(ImageType::Pointer image, const std::string& suffix);
};
