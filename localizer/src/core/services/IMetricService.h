#pragma once
#include "../common/ProcessingContracts.h"

namespace Pipeline {

class IMetricService {
public:
    virtual ~IMetricService() = default;

    virtual std::vector<MetricResult> calculate(const MetricComputationRequest& request) = 0;
};

} // namespace Pipeline

