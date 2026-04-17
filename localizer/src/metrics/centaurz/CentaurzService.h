#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../shared/MetricRunResult.h"
#include "../shared/MetricTypes.h"
#include <memory>
#include <string>

namespace Pipeline::Metrics::Centaurz {

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
    bool reportDetailedRegions = false;
    std::string debugOutputBasePath;
};

class CentaurzService {
public:
    CentaurzService(ConfigurationPtr config,
                    std::shared_ptr<ISpatialNormalizationService> spatialService,
                    std::shared_ptr<IFileService> fileService);

    int run(CentaurzCLIOptions options, const std::string& fullCommand);

private:
    Pipeline::Metrics::Shared::MetricRunResult executeForImage(const CentaurzCLIOptions& options,
                                                               const std::string& inputPath,
                                                               const std::string& outputPath,
                                                               const std::string& debugBasePath) const;
    int runSingle(const CentaurzCLIOptions& options, const std::string& fullCommand) const;
    int runBatch(const CentaurzCLIOptions& options, const std::string& fullCommand) const;
    static void logMetricResult(const Metrics::MetricResult& result, bool includeSUVr);

    ConfigurationPtr config_;
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<CentaurzService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::Centaurz
