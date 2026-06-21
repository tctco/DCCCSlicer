#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../shared/MetricTypes.h"
#include <memory>
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

class FillStatesService {
public:
    FillStatesService(ConfigurationPtr config,
                      std::shared_ptr<ISpatialNormalizationService> spatialService,
                      std::shared_ptr<IFileService> fileService);

    int run(FillStatesCLIOptions options, const std::string& fullCommand);

private:
    Metrics::MetricResult executeForImage(const FillStatesCLIOptions& options,
                                          const std::string& inputPath,
                                          const std::string& outputPath,
                                          const std::string& debugBasePath) const;
    int runSingle(const FillStatesCLIOptions& options, const std::string& fullCommand) const;
    static std::string buildMaskOutputPath(const std::string& outputPath);
    static void logMetricResult(const Metrics::MetricResult& result, bool includeSUVr);

    ConfigurationPtr config_;
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<FillStatesService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::FillStates
