#pragma once

#include "../../core/common/ImageTypes.h"
#include "../shared/MetricTypes.h"
#include <map>
#include <string>
#include <vector>

class CenTauRCalculator {
public:
    struct TracerParams {
        float baselineSuvr;
        float maxSuvr;
    };

    struct Input {
        ImageType::Pointer spatiallyNormalizedImage;
        std::string voiMaskPath;
        std::string refMaskPath;
        std::map<std::string, TracerParams> tracerParameters;
    };

    Pipeline::Metrics::MetricResult calculate(const Input& input) const;
};
