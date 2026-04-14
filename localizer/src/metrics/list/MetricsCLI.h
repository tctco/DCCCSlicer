#pragma once

#include "../shared/IMetricCLI.h"
#include "../shared/MetricRegistry.h"

namespace Pipeline::Metrics::List {

MetricCLIPtr createCLI();
void registerModule(MetricRegistry& registry);

} // namespace Pipeline::Metrics::List
