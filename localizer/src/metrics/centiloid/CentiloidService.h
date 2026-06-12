#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../../metrics/shared/MetricTypes.h"
#include <memory>
#include <string>

namespace Pipeline::Metrics::Centiloid {

struct CentiloidCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool includeSUVr = false;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableDebugOutput = false;
    bool batchMode = false;
    std::string bidsPattern;
    std::string debugOutputBasePath;
};

class CentiloidService {
public:
    CentiloidService(ConfigurationPtr config,
                     std::shared_ptr<ISpatialNormalizationService> spatialService,
                     std::shared_ptr<IFileService> fileService);

    int run(CentiloidCLIOptions options, const std::string& fullCommand);

private:
    Metrics::MetricResult executeForImage(const CentiloidCLIOptions& options,
                                          const std::string& inputPath,
                                          const std::string& outputPath,
                                          const std::string& debugBasePath) const;

    int runSingle(const CentiloidCLIOptions& options, const std::string& fullCommand) const;
    int runBatch(const CentiloidCLIOptions& options, const std::string& fullCommand) const;
    void logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) const;

    ConfigurationPtr config_;
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<CentiloidService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::Centiloid
