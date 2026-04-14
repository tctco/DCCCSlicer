#include "CentaurzService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "CenTauRzCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::Centaurz {

namespace {

constexpr const char* kLogTag = "centaurz";
constexpr const char* kBatchOutputSuffix = "_centaurz_refactor.nii";

std::map<std::string, CenTauRzCalculator::TracerParams> loadTracerParameters(ConfigurationPtr config) {
    if (!config) {
        throw std::invalid_argument("CenTauRz requires configuration");
    }

    std::map<std::string, CenTauRzCalculator::TracerParams> params;
    params["FTP"] = {config->getFloat("centaurz.tracers.ftp.slope", 13.63f),
                     config->getFloat("centaurz.tracers.ftp.intercept", -15.85f)};
    params["GTP1"] = {config->getFloat("centaurz.tracers.gtp1.slope", 10.67f),
                      config->getFloat("centaurz.tracers.gtp1.intercept", -11.92f)};
    params["MK6240"] = {config->getFloat("centaurz.tracers.mk6240.slope", 10.08f),
                        config->getFloat("centaurz.tracers.mk6240.intercept", -10.06f)};
    params["PI2620"] = {config->getFloat("centaurz.tracers.pi2620.slope", 8.45f),
                        config->getFloat("centaurz.tracers.pi2620.intercept", -9.61f)};
    params["RO948"] = {config->getFloat("centaurz.tracers.ro948.slope", 13.05f),
                       config->getFloat("centaurz.tracers.ro948.intercept", -15.57f)};
    params["PM-PBB3"] = {config->getFloat("centaurz.tracers.pmpbb3.slope", 16.73f),
                         config->getFloat("centaurz.tracers.pmpbb3.intercept", -15.34f)};
    return params;
}

SpatialNormalizationRequest buildNormalizationRequest(const CentaurzCLIOptions& options,
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

CentaurzService::CentaurzService(ConfigurationPtr config,
                                 std::shared_ptr<ISpatialNormalizationService> spatialService,
                                 std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("CentaurzService requires configuration, normalization, and file services");
    }
}

int CentaurzService::run(CentaurzCLIOptions options, const std::string& fullCommand) {
    if (!options.batchMode) {
        Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    }
    if (options.batchMode) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult CentaurzService::executeForImage(const CentaurzCLIOptions& options,
                                                       const std::string& inputPath,
                                                       const std::string& outputPath,
                                                       const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    CenTauRzCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.voiMaskPath = config_->getMaskPath("centaur_voi");
    input.refMaskPath = config_->getMaskPath("centaur_ref");
    input.tracerParameters = loadTracerParameters(config_);
    return CenTauRzCalculator().calculate(input);
}

int CentaurzService::runSingle(const CentaurzCLIOptions& options, const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<CentaurzCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const CentaurzCLIOptions& runnerOptions, const std::string&) {
        return runnerOptions.enableDebugOutput ? runnerOptions.debugOutputBasePath : std::string{};
    };
    hooks.execute = [this](const CentaurzCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const CentaurzCLIOptions& runnerOptions,
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

int CentaurzService::runBatch(const CentaurzCLIOptions& options, const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<CentaurzCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const CentaurzCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CentaurzCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const CentaurzCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void CentaurzService::logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) {
    std::cout << "\n=== Refactor CenTauRz Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [tracer, value] : result.tracerValues) {
        std::cout << "  " << tracer << ": " << value << std::endl;
    }
    if (includeSUVr) {
        std::cout << "  SUVr: " << result.suvr << std::endl;
    }
}

std::shared_ptr<CentaurzService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<CentaurzService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::Centaurz
