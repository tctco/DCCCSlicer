#pragma once
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/di/ServiceContainer.h"
#include <string>

namespace Pipeline::Metrics::AbetaLoad {

struct AbetaLoadCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableDebugOutput = false;
    bool batchMode = false;
    std::string debugOutputBasePath;
};

void registerMetric(ServiceContainer& container);
int runCommand(const AbetaLoadCLIOptions& options, const std::string& fullCommand);

} // namespace Pipeline::Metrics::AbetaLoad

