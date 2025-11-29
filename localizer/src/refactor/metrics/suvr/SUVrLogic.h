#pragma once
#include "../../interfaces/IMetricLogic.h"
#include "../../../cli/Options.h"
#include "../../../interfaces/IConfiguration.h"
#include "../../di/ServiceContainer.h"
#include <string>

namespace RefactorPipeline::Metrics::SUVr {

void registerMetric(ServiceContainer& container);
int runCommand(const SUVrCommandOptions& options, const std::string& fullCommand);

} // namespace RefactorPipeline::Metrics::SUVr
