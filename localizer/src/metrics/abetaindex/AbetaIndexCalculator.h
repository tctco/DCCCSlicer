#pragma once

#include "../../core/common/ImageTypes.h"
#include "../shared/MetricTypes.h"
#include <string>

class AbetaIndexCalculator {
public:
    struct Input {
        ImageType::Pointer spatiallyNormalizedImage;
        std::string meanTemplatePath;
        std::string pc1TemplatePath;
        std::string pc2TemplatePath;
        std::string tracer = "AV45";
    };

    Pipeline::Metrics::MetricResult calculate(const Input& input) const;
};
