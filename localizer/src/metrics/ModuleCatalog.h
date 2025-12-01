#pragma once
#include <vector>
#include "../core/interfaces/IMetricCLI.h"
#include "../core/di/ServiceContainer.h"

namespace Pipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules();
void registerAllMetricModules(ServiceContainer& container);

} // namespace Pipeline::Metrics

