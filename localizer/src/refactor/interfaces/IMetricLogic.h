#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../common/ProcessingContracts.h"

namespace RefactorPipeline {

class IMetricLogic {
public:
    virtual ~IMetricLogic() = default;
    virtual std::string getName() const = 0;
    virtual std::vector<MetricResult> calculate(const MetricComputationRequest& request) = 0;
};

using MetricPtr = std::shared_ptr<IMetricLogic>;

} // namespace RefactorPipeline

