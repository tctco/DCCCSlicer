#pragma once
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"
#include "../utils/common.h"
#include <string>
#include <vector>
/**
 * @brief Fill-states metric calculator
 *
 * Computes the proportion of suprathreshold voxels within a meta-ROI based on
 * voxel-wise z-score maps derived from tracer-specific mean/std templates.
 */
class FillStatesCalculator : public IMetricCalculator {
public:
    explicit FillStatesCalculator(ConfigurationPtr config);
    virtual ~FillStatesCalculator() = default;

    MetricResult calculate(ImageType::Pointer spatialNormalizedImage) override;
    std::string getName() const override;
    std::vector<std::string> getSupportedTracers() const override;

    /**
     * @brief Set tracer name used for this calculation.
     * Expected lower-case values: "fbp", "fdg", "ftp".
     */
    void setTracer(const std::string& tracer);

    /**
     * @brief Get the last computed fill-states mask image (0/1 float image).
     * May return nullptr if calculate has not been called or failed.
     */
    ImageType::Pointer getLastMaskImage() const;

private:
    ConfigurationPtr config_;
    std::string tracer_;                // lower-case tracer name
    ImageType::Pointer lastMaskImage_;  // cached mask image from last calculation

    struct TracerResources {
        std::string meanPath;
        std::string stdPath;
        std::string roiPath;      // direct path, may be empty if using config mask
        std::string roiMaskKey;   // config mask key, used when roiPath is empty
        std::string refMaskKey;   // reference mask key for intensity normalization
    };

    TracerResources getTracerResources() const;
    static std::string toLowerCopy(const std::string& v);
};


