#pragma once
#include "../../core/interfaces/IMetricCalculator.h"
#include "../../core/interfaces/IConfiguration.h"

/**
 * @brief AbetaLoad metric calculator that decomposes PET signal into NS and K components.
 */
class AbetaLoadCalculator : public IMetricCalculator {
public:
    explicit AbetaLoadCalculator(ConfigurationPtr config);
    ~AbetaLoadCalculator() override = default;

    MetricResult calculate(ImageType::Pointer spatialNormalizedImage) override;
    std::string getName() const override;
    std::vector<std::string> getSupportedTracers() const override;

private:
    ConfigurationPtr config_;

    struct TemplatePaths {
        std::string nsPath;
        std::string kPath;
    };

    TemplatePaths getTemplatePaths() const;
};
