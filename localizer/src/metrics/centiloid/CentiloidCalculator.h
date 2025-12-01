#pragma once
#include "../../core/interfaces/IMetricCalculator.h"
#include "../../core/interfaces/IConfiguration.h"

/**
 * @brief Centiloid metric calculator
 */
class CentiloidCalculator : public IMetricCalculator {
public:
    explicit CentiloidCalculator(ConfigurationPtr config);
    virtual ~CentiloidCalculator() = default;
    
    MetricResult calculate(ImageType::Pointer spatialNormalizedImage) override;
    std::string getName() const override;
    std::vector<std::string> getSupportedTracers() const override;
    
private:
    ConfigurationPtr config_;
    
    struct TracerParams {
        float slope;
        float intercept;
    };
    
    std::map<std::string, TracerParams> getTracerParameters() const;
};

