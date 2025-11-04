#pragma once
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"

/**
 * @brief CenTauR metric calculator (percentile-based formula)
 */
class CenTauRCalculator : public IMetricCalculator {
public:
    explicit CenTauRCalculator(ConfigurationPtr config);
    virtual ~CenTauRCalculator() = default;
    
    MetricResult calculate(ImageType::Pointer spatialNormalizedImage) override;
    std::string getName() const override;
    std::vector<std::string> getSupportedTracers() const override;
    
private:
    ConfigurationPtr config_;
    
    struct TracerParams {
        float baselineSuvr;
        float maxSuvr;
    };
    
    std::map<std::string, TracerParams> getTracerParameters() const;
};

