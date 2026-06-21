#pragma once

#include "../../core/common/ImageTypes.h"
#include "../shared/MetricTypes.h"
#include <map>
#include <string>
#include <vector>

class CenTauRzCalculator {
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
        struct DetailedRegion {
            std::string metricName;
            std::string voiMaskPath;
            std::map<std::string, TracerParams> tracerParameters;
        };
        std::vector<DetailedRegion> detailedRegions;
    };

    Pipeline::Metrics::MetricResult calculate(const Input& input) const;
    std::vector<Pipeline::Metrics::MetricResult> calculateDetailedRegions(const Input& input) const;

private:
    Pipeline::Metrics::MetricResult calculateRegion(const std::string& metricName,
                                                    ImageType::Pointer spatiallyNormalizedImage,
                                                    const std::string& voiMaskPath,
                                                    const std::string& refMaskPath,
                                                    const std::map<std::string, TracerParams>& tracerParameters) const;
};
