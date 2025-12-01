#pragma once
#include "../../core/di/ServiceContainer.h"
#include <string>

namespace Pipeline::Metrics::FillStates {

struct FillStatesCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    std::string tracer;
    bool includeSUVr = false;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableDebugOutput = false;
    bool batchMode = false;
    std::string debugOutputBasePath;
};

void registerMetric(ServiceContainer& container);
int runCommand(const FillStatesCLIOptions& options, const std::string& fullCommand);

} // namespace Pipeline::Metrics::FillStates


