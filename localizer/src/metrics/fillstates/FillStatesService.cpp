#include "FillStatesService.h"

#include "../../core/common/Common.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "FillStatesCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::FillStates {

namespace {

constexpr const char* kLogTag = "fillstates";

SpatialNormalizationRequest buildNormalizationRequest(const FillStatesCLIOptions& options,
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

FillStatesService::FillStatesService(ConfigurationPtr config,
                                     std::shared_ptr<ISpatialNormalizationService> spatialService,
                                     std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("FillStatesService requires configuration, normalization, and file services");
    }
}

int FillStatesService::run(FillStatesCLIOptions options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[fillstates] Batch mode is not supported." << std::endl;
        return EXIT_FAILURE;
    }
    if (options.tracer.empty()) {
        std::cerr << "[fillstates] --tracer is required." << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    return runSingle(options, fullCommand);
}

Metrics::MetricResult FillStatesService::executeForImage(const FillStatesCLIOptions& options,
                                                         const std::string& inputPath,
                                                         const std::string& outputPath,
                                                         const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    FillStatesCalculator calculator(config_);
    calculator.setTracer(options.tracer);
    auto result = calculator.calculate(normalizationOutput.spatiallyNormalizedImage);

    const std::string maskOutputPath = buildMaskOutputPath(outputPath);
    if (auto mask = calculator.getLastMaskImage(); mask && !maskOutputPath.empty()) {
        Common::fs::ensureParentDirectory(maskOutputPath);
        Common::nifti::saveImage(mask, maskOutputPath);
        std::cout << "[fillstates] Fill-states mask saved to " << maskOutputPath << std::endl;
    }

    return result;
}

int FillStatesService::runSingle(const FillStatesCLIOptions& options,
                                 const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<FillStatesCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const FillStatesCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const FillStatesCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const FillStatesCLIOptions& runnerOptions,
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

std::string FillStatesService::buildMaskOutputPath(const std::string& outputPath) {
    if (outputPath.empty()) {
        return {};
    }
    return Common::path::addSuffix(outputPath, "_fill_states_map");
}

void FillStatesService::logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) {
    std::cout << "\n=== Refactor FillStates Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [tracer, value] : result.tracerValues) {
        std::cout << "  " << tracer << ": " << value << std::endl;
    }
    if (includeSUVr) {
        std::cout << "  SUVr: " << result.suvr << std::endl;
    }
}

std::shared_ptr<FillStatesService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<FillStatesService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::FillStates
