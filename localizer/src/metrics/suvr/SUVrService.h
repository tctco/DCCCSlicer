#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../shared/MetricTypes.h"
#include <memory>
#include <string>

namespace Pipeline::Metrics::SUVr {

struct SUVrCLIOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool batchMode = false;
    std::string bidsPattern;
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    std::string voiMaskPath;
    std::string refMaskPath;
};

class SUVrService {
public:
    SUVrService(std::shared_ptr<ISpatialNormalizationService> spatialService,
                std::shared_ptr<IFileService> fileService);

    int run(SUVrCLIOptions options, const std::string& fullCommand);

private:
    Metrics::MetricResult executeForImage(const SUVrCLIOptions& options,
                                          const std::string& inputPath,
                                          const std::string& outputPath,
                                          const std::string& debugBasePath) const;
    int runSingle(const SUVrCLIOptions& options, const std::string& fullCommand) const;
    int runBatch(const SUVrCLIOptions& options, const std::string& fullCommand) const;
    static void logMetricResult(const Metrics::MetricResult& result);

    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<SUVrService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::SUVr
