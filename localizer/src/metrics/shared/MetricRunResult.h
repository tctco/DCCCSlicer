#pragma once

#include "MetricTypes.h"
#include <vector>

namespace Pipeline::Metrics::Shared {

struct MetricRunResult {
    std::vector<Pipeline::Metrics::MetricResult> metricResults;
};

} // namespace Pipeline::Metrics::Shared
