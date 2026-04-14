#include "AbetaLoadService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "AbetaLoadCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::AbetaLoad {

namespace {

constexpr const char* kLogTag = "abetaload";
constexpr const char* kBatchOutputSuffix = "_abetaload.nii";

SpatialNormalizationRequest buildNormalizationRequest(const AbetaLoadCLIOptions& options,
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

AbetaLoadService::AbetaLoadService(ConfigurationPtr config,
                                   std::shared_ptr<ISpatialNormalizationService> spatialService,
                                   std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("AbetaLoadService requires configuration, normalization, and file services");
    }
}

int AbetaLoadService::run(AbetaLoadCLIOptions options, const std::string& fullCommand) {
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    if (options.batchMode) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult AbetaLoadService::executeForImage(const AbetaLoadCLIOptions& options,
                                                        const std::string& inputPath,
                                                        const std::string& outputPath,
                                                        const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    AbetaLoadCalculator calculator(config_);
    return calculator.calculate(normalizationOutput.spatiallyNormalizedImage);
}

int AbetaLoadService::runSingle(const AbetaLoadCLIOptions& options,
                                const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<AbetaLoadCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const AbetaLoadCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const AbetaLoadCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const AbetaLoadCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        std::cout << "\n[" << kLogTag
                  << "] Spatial normalization complete. Normalized image saved to "
                  << runnerOptions.outputPath << std::endl;
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric);
        }
    };
    return Pipeline::Metrics::Shared::runSingle(options, fullCommand, hooks);
}

int AbetaLoadService::runBatch(const AbetaLoadCLIOptions& options,
                               const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<AbetaLoadCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const AbetaLoadCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const AbetaLoadCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const AbetaLoadCLIOptions&,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void AbetaLoadService::logMetricResult(const Metrics::MetricResult& result) {
    std::cout << "\n=== AbetaLoad Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [label, value] : result.tracerValues) {
        std::cout << "  " << label << ": " << value << std::endl;
    }
    std::cout << "  Abeta_load (SUVr field): " << result.suvr << std::endl;
}

std::shared_ptr<AbetaLoadService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<AbetaLoadService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::AbetaLoad
