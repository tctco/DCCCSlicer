#include "SUVrService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "SUVrCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::SUVr {

namespace {

constexpr const char* kLogTag = "suvr";
constexpr const char* kBatchOutputSuffix = "_suvr.nii";

SpatialNormalizationRequest buildNormalizationRequest(const SUVrCLIOptions& options,
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

SUVrService::SUVrService(std::shared_ptr<ISpatialNormalizationService> spatialService,
                         std::shared_ptr<IFileService> fileService)
    : spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!spatialService_ || !fileService_) {
        throw std::invalid_argument("SUVrService requires normalization and file services");
    }
}

int SUVrService::run(SUVrCLIOptions options, const std::string& fullCommand) {
    if (options.voiMaskPath.empty() || options.refMaskPath.empty()) {
        std::cerr << "[suvr] Both --voi-mask and --ref-mask must be provided." << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    if (options.batchMode || !options.bidsPattern.empty()) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult SUVrService::executeForImage(const SUVrCLIOptions& options,
                                                   const std::string& inputPath,
                                                   const std::string& outputPath,
                                                   const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    SUVrCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.voiMaskPath = options.voiMaskPath;
    input.refMaskPath = options.refMaskPath;
    return SUVrCalculator().calculate(input);
}

int SUVrService::runSingle(const SUVrCLIOptions& options, const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<SUVrCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const SUVrCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const SUVrCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const SUVrCLIOptions& runnerOptions,
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

int SUVrService::runBatch(const SUVrCLIOptions& options, const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<SUVrCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const SUVrCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const SUVrCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const SUVrCLIOptions&,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void SUVrService::logMetricResult(const Metrics::MetricResult& result) {
    std::cout << "\n=== SUVr Results ===" << std::endl;
    std::cout << result.metricName << ": " << result.suvr << std::endl;
}

std::shared_ptr<SUVrService> createService(ServiceContainer& container) {
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<SUVrService>(spatialService, fileService);
}

} // namespace Pipeline::Metrics::SUVr
