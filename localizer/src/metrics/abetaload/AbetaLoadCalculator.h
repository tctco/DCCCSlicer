#pragma once

#include "../../core/common/ImageTypes.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../shared/MetricTypes.h"

class AbetaLoadCalculator {
public:
    explicit AbetaLoadCalculator(ConfigurationPtr config);
    ~AbetaLoadCalculator() = default;

    Pipeline::Metrics::MetricResult calculate(ImageType::Pointer spatialNormalizedImage);

private:
    ConfigurationPtr config_;

    struct TemplatePaths {
        std::string nsPath;
        std::string kPath;
    };

    TemplatePaths getTemplatePaths() const;
};
