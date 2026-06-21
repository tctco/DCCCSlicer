#pragma once

#include "../../core/common/ImageTypes.h"
#include "../shared/MetricTypes.h"
#include <map>
#include <string>
#include <vector>

class CentiloidCalculator {
public:
    struct TracerParams {
        float slope;
        float intercept;
    };

    struct Input {
        ImageType::Pointer spatiallyNormalizedImage;
        std::string voiMaskPath;
        std::string refMaskPath;
        std::map<std::string, TracerParams> tracerParameters;
    };

    Pipeline::Metrics::MetricResult calculate(const Input& input) const;
    static std::vector<std::string> supportedTracers();
};
