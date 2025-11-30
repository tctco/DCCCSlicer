#include "MetricService.h"
#include "../common/Common.h"
#include <string>
#include <stdexcept>
#include <utility>

namespace RefactorPipeline {

MetricService::MetricService(std::shared_ptr<IMetricModuleRegistry> registry)
    : registry_(std::move(registry)) {
    if (!registry_) {
        throw std::invalid_argument("MetricService requires a metric module registry");
    }
}

std::vector<MetricResult> MetricService::calculate(const MetricComputationRequest& request) {
    if (!request.spatiallyNormalizedImage) {
        throw std::invalid_argument("MetricService requires a spatially normalized image");
    }

    std::string metricName = refactorCommon::path::toLower(request.options.metricName);
    if (metricName.empty()) {
        return {};
    }

    return registry_->run(metricName, request);
}

} // namespace RefactorPipeline

