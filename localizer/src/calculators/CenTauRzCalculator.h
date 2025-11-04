#pragma once
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"

/**
 * @brief CenTauRz metric calculator (z-score based formula)
 */
class CenTauRzCalculator : public IMetricCalculator {
public:
    explicit CenTauRzCalculator(ConfigurationPtr config);
    virtual ~CenTauRzCalculator() = default;
    
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
