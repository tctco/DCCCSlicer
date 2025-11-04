#pragma once
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"

/**
 * @brief SUVr (Standardized Uptake Value ratio) metric calculator
 * Base calculator that computes SUVr values using configurable VOI and reference regions
 */
class SUVrCalculator : public IMetricCalculator {
public:
    explicit SUVrCalculator(ConfigurationPtr config);
    virtual ~SUVrCalculator() = default;
    
    MetricResult calculate(ImageType::Pointer spatialNormalizedImage) override;
    std::string getName() const override;
    std::vector<std::string> getSupportedTracers() const override;
    
    // Static utility method for other calculators to use
    static double calculateSUVr(ImageType::Pointer spatialNormalizedImage, 
                               const std::string& voiMaskPath, 
                               const std::string& refMaskPath);
    
private:
    ConfigurationPtr config_;
    
    struct SUVrRegionConfig {
        std::string voiMaskKey;
        std::string refMaskKey;
        std::string description;
    };
    
    std::vector<SUVrRegionConfig> getRegionConfigurations() const;
};
