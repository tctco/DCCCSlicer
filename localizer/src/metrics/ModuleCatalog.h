#pragma once

#include <string>
#include <vector>
#include "shared/IMetricCLI.h"

namespace Pipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules();
std::vector<std::string> listMetricNames();

} // namespace Pipeline::Metrics
