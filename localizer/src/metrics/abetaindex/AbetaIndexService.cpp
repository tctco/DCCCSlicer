#include "AbetaIndexService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/PathUtils.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "AbetaIndexCalculator.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::AbetaIndex {

namespace {

constexpr const char* kLogTag = "abetaindex";
constexpr const char* kBatchOutputSuffix = "_abetaindex.nii";

void validateAbetaIndexConfiguration(ConfigurationPtr config) {
    if (!config->getBool("abetaindex.enabled", false)) {
        throw std::runtime_error(
            "AbetaIndex is not enabled for the current config. Use the standard config.toml "
            "with abetaindex settings; fast_and_acc is not supported.");
    }

    if (config->getString("templates.abeta_index_mean").empty() ||
        config->getString("templates.abeta_index_pc1").empty() ||
        config->getString("templates.abeta_index_pc2").empty()) {
        throw std::runtime_error(
            "AbetaIndex templates are missing from the current config. "
            "Expected templates.abeta_index_mean/pc1/pc2.");
    }

    const std::string meanPath = config->getTemplatePath("abeta_index_mean");
    const std::string pc1Path = config->getTemplatePath("abeta_index_pc1");
    const std::string pc2Path = config->getTemplatePath("abeta_index_pc2");
    if (!std::filesystem::exists(Common::path::fromUtf8(meanPath)) ||
        !std::filesystem::is_regular_file(Common::path::fromUtf8(meanPath)) ||
        !std::filesystem::exists(Common::path::fromUtf8(pc1Path)) ||
        !std::filesystem::is_regular_file(Common::path::fromUtf8(pc1Path)) ||
        !std::filesystem::exists(Common::path::fromUtf8(pc2Path)) ||
        !std::filesystem::is_regular_file(Common::path::fromUtf8(pc2Path))) {
        throw std::runtime_error(
            "AbetaIndex template files are missing for the active configuration. "
            "Please ensure the selected config defines templates.abeta_index_mean/pc1/pc2 "
            "and that the referenced files exist.");
    }
}

SpatialNormalizationRequest buildNormalizationRequest(const AbetaIndexCLIOptions& options,
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

AbetaIndexService::AbetaIndexService(ConfigurationPtr config,
                                     std::shared_ptr<ISpatialNormalizationService> spatialService,
                                     std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("AbetaIndexService requires configuration, normalization, and file services");
    }
}

int AbetaIndexService::run(AbetaIndexCLIOptions options, const std::string& fullCommand) {
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    if (options.batchMode) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Metrics::MetricResult AbetaIndexService::executeForImage(const AbetaIndexCLIOptions& options,
                                                         const std::string& inputPath,
                                                         const std::string& outputPath,
                                                         const std::string& debugBasePath) const {
    validateAbetaIndexConfiguration(config_);

    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    AbetaIndexCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.meanTemplatePath = config_->getTemplatePath("abeta_index_mean");
    input.pc1TemplatePath = config_->getTemplatePath("abeta_index_pc1");
    input.pc2TemplatePath = config_->getTemplatePath("abeta_index_pc2");
    input.tracer = config_->getString("abetaindex.tracer", "AV45");
    return AbetaIndexCalculator().calculate(input);
}

int AbetaIndexService::runSingle(const AbetaIndexCLIOptions& options,
                                 const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<AbetaIndexCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const AbetaIndexCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput ? Common::path::deriveDebugBasePath(outputPath)
                                               : std::string{};
    };
    hooks.execute = [this](const AbetaIndexCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const AbetaIndexCLIOptions& runnerOptions,
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

int AbetaIndexService::runBatch(const AbetaIndexCLIOptions& options,
                                const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<AbetaIndexCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const AbetaIndexCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput ? Common::path::deriveDebugBasePath(outputPath)
                                               : std::string{};
    };
    hooks.execute = [this](const AbetaIndexCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const AbetaIndexCLIOptions&,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void AbetaIndexService::logMetricResult(const Metrics::MetricResult& result) {
    std::cout << "\n=== AbetaIndex Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [label, value] : result.tracerValues) {
        std::cout << "  " << label << ": " << value << std::endl;
    }
    std::cout << "  AbetaIndex: " << result.suvr << std::endl;
}

std::shared_ptr<AbetaIndexService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<AbetaIndexService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::AbetaIndex
