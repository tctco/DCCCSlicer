#pragma once

#include "../../core/common/ImageTypes.h"
#include "../shared/MetricTypes.h"
#include <map>
#include <string>

class SUVrCalculator {
public:
    struct Input {
        ImageType::Pointer spatiallyNormalizedImage;
        std::string voiMaskPath;
        std::string refMaskPath;
    };

    Pipeline::Metrics::MetricResult calculate(const Input& input) const;
    static double calculateSUVr(ImageType::Pointer spatialNormalizedImage,
                                const std::string& voiMaskPath,
                                const std::string& refMaskPath);
};
