#pragma once

#include "../../core/common/ImageTypes.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../shared/MetricTypes.h"
#include <string>
#include <vector>
/**
 * @brief Fill-states metric calculator
 *
 * Computes the proportion of suprathreshold voxels within a meta-ROI based on
 * voxel-wise z-score maps derived from tracer-specific mean/std templates.
 */
class FillStatesCalculator {
public:
    explicit FillStatesCalculator(ConfigurationPtr config);
    ~FillStatesCalculator() = default;

    Pipeline::Metrics::MetricResult calculate(ImageType::Pointer spatialNormalizedImage);

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

