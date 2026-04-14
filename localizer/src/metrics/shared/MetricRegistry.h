#pragma once

#include "IMetricCLI.h"
#include <functional>
#include <string>
#include <vector>

namespace Pipeline::Metrics {

struct MetricModuleRegistration {
    std::string name;
    bool includeInMetricList = true;
    std::function<MetricCLIPtr()> createCLI;
};

class MetricRegistry {
public:
    void registerModule(MetricModuleRegistration module);
    std::vector<MetricCLIPtr> createCLIModules() const;
    std::vector<std::string> listMetricNames() const;

private:
    std::vector<MetricModuleRegistration> modules_;
};

} // namespace Pipeline::Metrics
