#pragma once
#include "../../interfaces/IMetricLogic.h"
#include "../../../interfaces/IConfiguration.h"
#include "../../di/ServiceContainer.h"
#include <string>

namespace RefactorPipeline::Metrics::ADAD {

struct ADADCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool batchMode = false;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    std::string modality = "abeta";
};

void registerMetric(ServiceContainer& container);
int runCommand(const ADADCLIOptions& options, const std::string& fullCommand);

} // namespace RefactorPipeline::Metrics::ADAD


