#include "MetricRegistry.h"

#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics {

void MetricRegistry::registerModule(MetricModuleRegistration module) {
    if (module.name.empty()) {
        throw std::invalid_argument("Metric module name must be non-empty");
    }
    if (!module.createCLI) {
        throw std::invalid_argument("Metric module must provide a CLI factory");
    }

    for (const auto& existing : modules_) {
        if (existing.name == module.name) {
            throw std::invalid_argument("Metric module already registered: " + module.name);
        }
    }

    modules_.push_back(std::move(module));
}

std::vector<MetricCLIPtr> MetricRegistry::createCLIModules() const {
    std::vector<MetricCLIPtr> modules;
    modules.reserve(modules_.size());
    for (const auto& registration : modules_) {
        modules.push_back(registration.createCLI());
    }
    return modules;
}

std::vector<std::string> MetricRegistry::listMetricNames() const {
    std::vector<std::string> names;
    for (const auto& registration : modules_) {
        if (registration.includeInMetricList) {
            names.push_back(registration.name);
        }
    }
    return names;
}

} // namespace Pipeline::Metrics
