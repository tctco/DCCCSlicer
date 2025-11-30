#include "MetricModuleRegistry.h"
#include <algorithm>
#include <stdexcept>

namespace RefactorPipeline {

void MetricModuleRegistry::registerModule(const MetricPtr& module) {
    if (!module) {
        throw std::invalid_argument("Cannot register null metric module");
    }
    std::string key = normalizeName(module->getName());
    if (key.empty()) {
        throw std::invalid_argument("Metric module must provide a non-empty name");
    }
    if (modules_.count(key) > 0) {
        throw std::invalid_argument("Metric module already registered: " + module->getName());
    }
    modules_[key] = module;
}

std::vector<MetricResult> MetricModuleRegistry::run(const std::string& metricName,
                                                    const MetricComputationRequest& request) const {
    std::string key = normalizeName(metricName);
    auto it = modules_.find(key);
    if (it == modules_.end()) {
        throw std::runtime_error("Metric module not registered: " + metricName);
    }
    return it->second->calculate(request);
}

bool MetricModuleRegistry::hasModule(const std::string& metricName) const {
    std::string key = normalizeName(metricName);
    return modules_.find(key) != modules_.end();
}

std::vector<std::string> MetricModuleRegistry::listModules() const {
    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const auto& [key, module] : modules_) {
        names.push_back(module->getName());
    }
    return names;
}

std::string MetricModuleRegistry::normalizeName(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

} // namespace RefactorPipeline

