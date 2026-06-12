#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../shared/MetricTypes.h"
#include <memory>
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
    std::string bidsPattern;
    std::string debugOutputBasePath;
};

class AbetaLoadService {
public:
    AbetaLoadService(ConfigurationPtr config,
                     std::shared_ptr<ISpatialNormalizationService> spatialService,
                     std::shared_ptr<IFileService> fileService);

    int run(AbetaLoadCLIOptions options, const std::string& fullCommand);

private:
    Metrics::MetricResult executeForImage(const AbetaLoadCLIOptions& options,
                                          const std::string& inputPath,
                                          const std::string& outputPath,
                                          const std::string& debugBasePath) const;
    int runSingle(const AbetaLoadCLIOptions& options, const std::string& fullCommand) const;
    int runBatch(const AbetaLoadCLIOptions& options, const std::string& fullCommand) const;
    static void logMetricResult(const Metrics::MetricResult& result);

    ConfigurationPtr config_;
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<AbetaLoadService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::AbetaLoad
