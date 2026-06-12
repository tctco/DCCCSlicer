#include "CentiloidService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../core/config/Version.h"
#include "../../metrics/shared/BatchLogging.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "CentiloidCalculator.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::Centiloid {

namespace {

constexpr const char* kLogTag = "centiloid";
constexpr const char* kBatchOutputSuffix = "_centiloid.nii";

std::map<std::string, CentiloidCalculator::TracerParams> loadTracerParameters(ConfigurationPtr config) {
    if (!config) {
        throw std::invalid_argument("Centiloid requires configuration");
    }

    std::map<std::string, CentiloidCalculator::TracerParams> params;
    for (const auto& tracer : CentiloidCalculator::supportedTracers()) {
        std::string tracerLower = tracer;
        std::transform(
            tracerLower.begin(),
            tracerLower.end(),
            tracerLower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        params[tracer] = CentiloidCalculator::TracerParams{
            config->getFloat("centiloid.tracers." + tracerLower + ".slope"),
            config->getFloat("centiloid.tracers." + tracerLower + ".intercept"),
        };
    }

    return params;
}

SpatialNormalizationRequest buildNormalizationRequest(const CentiloidCLIOptions& options,
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

CentiloidService::CentiloidService(ConfigurationPtr config,
                                   std::shared_ptr<ISpatialNormalizationService> spatialService,
                                   std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("CentiloidService requires configuration, normalization, and file services");
    }
}

int CentiloidService::run(CentiloidCLIOptions options, const std::string& fullCommand) {
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    if (options.batchMode || !options.bidsPattern.empty()) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult CentiloidService::executeForImage(const CentiloidCLIOptions& options,
                                                        const std::string& inputPath,
                                                        const std::string& outputPath,
                                                        const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    CentiloidCalculator calculator;
    CentiloidCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.voiMaskPath = config_->getMaskPath("centiloid_voi");
    input.refMaskPath = config_->getMaskPath("whole_cerebral");
    input.tracerParameters = loadTracerParameters(config_);
    return calculator.calculate(input);
}

int CentiloidService::runSingle(const CentiloidCLIOptions& options,
                                const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<CentiloidCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const CentiloidCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CentiloidCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [this](const CentiloidCLIOptions& runnerOptions,
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

int CentiloidService::runBatch(const CentiloidCLIOptions& options,
                               const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<CentiloidCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const CentiloidCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CentiloidCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [this](const CentiloidCLIOptions& runnerOptions,
                              const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void CentiloidService::logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) const {
    std::cout << "\n=== Centiloid Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [tracer, value] : result.tracerValues) {
        std::cout << "  " << tracer << ": " << value << std::endl;
    }
    if (includeSUVr) {
        std::cout << "  SUVr: " << result.suvr << std::endl;
    }
}

std::shared_ptr<CentiloidService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<CentiloidService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::Centiloid
