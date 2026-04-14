#pragma once

#include "../../core/di/ServiceContainer.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../shared/MetricTypes.h"
#include "Decoupler.h"
#include <map>
#include <memory>
#include <string>

namespace Pipeline::Metrics::ADAD {

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

class ADADService {
public:
    ADADService(ConfigurationPtr config,
                std::shared_ptr<ISpatialNormalizationService> spatialService,
                std::shared_ptr<IFileService> fileService);

    int run(ADADCLIOptions options, const std::string& fullCommand);

private:
    Metrics::MetricResult executeForImage(const ADADCLIOptions& options,
                                          const std::string& inputPath,
                                          const std::string& outputPath,
                                          const std::string& debugBasePath) const;
    int runSingle(const ADADCLIOptions& options, const std::string& fullCommand) const;
    std::map<std::string, float> buildTracerValues(const std::string& modality,
                                                   const DecoupledResult& decoupled) const;
    std::map<std::string, std::pair<float, float>> loadTracerCoefficients(const std::string& modality) const;
    static std::string normalizeModality(const std::string& raw);
    static void logMetricResult(const Metrics::MetricResult& result, const std::string& modality);

    ConfigurationPtr config_;
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IFileService> fileService_;
};

std::shared_ptr<ADADService> createService(ServiceContainer& container);

} // namespace Pipeline::Metrics::ADAD
