#pragma once
#include <vector>
#include "../interfaces/IMetricCLI.h"

namespace RefactorPipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules();

} // namespace RefactorPipeline::Metrics

