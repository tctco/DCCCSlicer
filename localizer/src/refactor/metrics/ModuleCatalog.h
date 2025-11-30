#pragma once
#include <vector>
#include "../interfaces/IMetricCLI.h"
#include "../di/ServiceContainer.h"

namespace RefactorPipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules();
void registerAllMetricModules(ServiceContainer& container);

} // namespace RefactorPipeline::Metrics

