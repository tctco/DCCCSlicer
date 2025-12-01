#pragma once
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/di/ServiceContainer.h"
#include <string>

namespace Pipeline::Metrics::SUVr {

struct SUVrCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool batchMode = false;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    std::string voiMaskPath;
    std::string refMaskPath;
};

void registerMetric(ServiceContainer& container);
int runCommand(const SUVrCLIOptions& options, const std::string& fullCommand);

} // namespace Pipeline::Metrics::SUVr
