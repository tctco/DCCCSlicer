#include "CenTauRService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "CenTauRCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::Centaur {

namespace {

constexpr const char* kLogTag = "centaur";
constexpr const char* kBatchOutputSuffix = "_centaur_refactor.nii";

std::map<std::string, CenTauRCalculator::TracerParams> loadTracerParameters(ConfigurationPtr config) {
    if (!config) {
        throw std::invalid_argument("CenTauR requires configuration");
    }

    std::map<std::string, CenTauRCalculator::TracerParams> params;
    params["FTP"] = {config->getFloat("centaur.tracers.ftp.baseline", 1.06f),
                     config->getFloat("centaur.tracers.ftp.max", 2.13f)};
    params["GTP1"] = {config->getFloat("centaur.tracers.gtp1.baseline", 1.08f),
                      config->getFloat("centaur.tracers.gtp1.max", 1.69f)};
    params["MK6240"] = {config->getFloat("centaur.tracers.mk6240.baseline", 0.93f),
                        config->getFloat("centaur.tracers.mk6240.max", 3.30f)};
    params["PI2620"] = {config->getFloat("centaur.tracers.pi2620.baseline", 1.17f),
                        config->getFloat("centaur.tracers.pi2620.max", 2.12f)};
    params["RO948"] = {config->getFloat("centaur.tracers.ro948.baseline", 1.03f),
                       config->getFloat("centaur.tracers.ro948.max", 2.40f)};
    return params;
}

SpatialNormalizationRequest buildNormalizationRequest(const CenTauRCLIOptions& options,
                                                      const std::string& inputPath,
                                                      const std::string& debugBasePath) {
    SpatialNormalizationRequest request;
    request.inputPath = inputPath;
    request.skip = options.skipRegistration;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.useManualFOV = options.useManualFOV;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = options.enableDebugOutput ? debugBasePath : std::string{};
    return request;
}

} // namespace

CenTauRService::CenTauRService(ConfigurationPtr config,
                               std::shared_ptr<ISpatialNormalizationService> spatialService,
                               std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("CenTauRService requires configuration, normalization, and file services");
    }
}

int CenTauRService::run(CenTauRCLIOptions options, const std::string& fullCommand) {
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    if (options.batchMode) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult CenTauRService::executeForImage(const CenTauRCLIOptions& options,
                                                      const std::string& inputPath,
                                                      const std::string& outputPath,
                                                      const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    CenTauRCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.voiMaskPath = config_->getMaskPath("centaur_voi");
    input.refMaskPath = config_->getMaskPath("centaur_ref");
    input.tracerParameters = loadTracerParameters(config_);
    return CenTauRCalculator().calculate(input);
}

int CenTauRService::runSingle(const CenTauRCLIOptions& options, const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<CenTauRCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const CenTauRCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CenTauRCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const CenTauRCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        std::cout << "\n[" << kLogTag
                  << "] Spatial normalization complete. Normalized image saved to "
                  << runnerOptions.outputPath << std::endl;
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runSingle(options, fullCommand, hooks);
}

int CenTauRService::runBatch(const CenTauRCLIOptions& options, const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<CenTauRCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const CenTauRCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CenTauRCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const CenTauRCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void CenTauRService::logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) {
    std::cout << "\n=== Refactor CenTauR Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [tracer, value] : result.tracerValues) {
        std::cout << "  " << tracer << ": " << value << std::endl;
    }
    if (includeSUVr) {
        std::cout << "  SUVr: " << result.suvr << std::endl;
    }
}

std::shared_ptr<CenTauRService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<CenTauRService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::Centaur
