#pragma once
#include "../../di/ServiceContainer.h"
#include <string>

namespace RefactorPipeline::Metrics::Centaurz {

struct CentaurzCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool includeSUVr = false;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableDebugOutput = false;
    bool batchMode = false;
    std::string debugOutputBasePath;
};

void registerMetric(ServiceContainer& container);
int runCommand(const CentaurzCLIOptions& options, const std::string& fullCommand);

} // namespace RefactorPipeline::Metrics::Centaurz


