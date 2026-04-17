#pragma once

#include "../shared/IMetricCLI.h"

namespace Pipeline::Metrics::Centaurz {

inline constexpr const char* kDetailedRegionReportFlag = "--report-detailed-regions";

MetricCLIPtr createCLI();

} // namespace Pipeline::Metrics::Centaurz
