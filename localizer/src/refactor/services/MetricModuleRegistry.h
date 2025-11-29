#pragma once
#include "../interfaces/IMetricModuleRegistry.h"
#include <unordered_map>

namespace RefactorPipeline {

class MetricModuleRegistry : public IMetricModuleRegistry {
public:
    MetricModuleRegistry() = default;
    ~MetricModuleRegistry() override = default;

    void registerModule(const MetricPtr& module) override;
    std::vector<MetricResult> run(const std::string& metricName,
                                  const MetricComputationRequest& request) const override;
    bool hasModule(const std::string& metricName) const override;
    std::vector<std::string> listModules() const override;

private:
    static std::string normalizeName(const std::string& name);

    std::unordered_map<std::string, MetricPtr> modules_;
};

} // namespace RefactorPipeline

