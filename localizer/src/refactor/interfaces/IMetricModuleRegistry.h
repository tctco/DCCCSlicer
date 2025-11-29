#pragma once
#include <string>
#include <vector>
#include "IMetricLogic.h"

namespace RefactorPipeline {

class IMetricModuleRegistry {
public:
    virtual ~IMetricModuleRegistry() = default;

    virtual void registerModule(const MetricPtr& module) = 0;
    virtual std::vector<MetricResult> run(const std::string& metricName,
                                          const MetricComputationRequest& request) const = 0;
    virtual bool hasModule(const std::string& metricName) const = 0;
    virtual std::vector<std::string> listModules() const = 0;
};

using MetricModuleRegistryPtr = std::shared_ptr<IMetricModuleRegistry>;

} // namespace RefactorPipeline

