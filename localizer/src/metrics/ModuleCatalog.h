#pragma once
#include <vector>
#include "../core/interfaces/IMetricCLI.h"
#include "../core/di/ServiceContainer.h"

namespace RefactorPipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules();
void registerAllMetricModules(ServiceContainer& container);

} // namespace RefactorPipeline::Metrics

