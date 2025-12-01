#pragma once
#include "IMetricService.h"
#include "../interfaces/IMetricModuleRegistry.h"
#include <vector>

namespace Pipeline {

class MetricService : public IMetricService {
public:
    explicit MetricService(std::shared_ptr<IMetricModuleRegistry> registry);

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override;

private:
    std::shared_ptr<IMetricModuleRegistry> registry_;
};

} // namespace Pipeline

