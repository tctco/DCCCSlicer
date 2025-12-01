#include "CentiloidLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "CentiloidCalculator.h"
#include "../../core/common/BatchLogging.h"
#include "../../core/config/Version.h"
#include "../../core/interfaces/IConfiguration.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace RefactorPipeline::Metrics::Centiloid {

namespace {

void configureDebugOutputBasePath(CentiloidCLIOptions& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        return;
    }
    options.debugOutputBasePath = refactorCommon::path::deriveDebugBasePath(options.outputPath);
}

class CentiloidLogic : public IMetricLogic {
public:
    explicit CentiloidLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("CentiloidLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "centiloid";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        CentiloidCalculator calculator(config_);
        MetricResult result = calculator.calculate(request.spatiallyNormalizedImage);
        return {result};
    }

private:
    ConfigurationPtr config_;
};

ProcessingRequest buildProcessingRequest(const CentiloidCLIOptions& options,
                                         const std::string& inputPath,
                                         const std::string& outputPath,
                                         const std::string& debugBasePath) {
    ProcessingRequest request;
    request.outputPath = outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "centiloid";

    request.normalization.inputPath = inputPath;
    request.normalization.skip = options.skipRegistration;
    request.normalization.options.useIterativeRigid = options.useIterativeRigid;
    request.normalization.options.useManualFOV = options.useManualFOV;
    request.normalization.options.enableDebugOutput = options.enableDebugOutput;
    request.normalization.options.debugOutputBasePath =
        options.enableDebugOutput ? debugBasePath : std::string{};
    return request;
}

void logMetricResults(const ProcessingResponse& response, bool includeSUVr) {
    if (response.metricResults.empty()) {
        std::cout << "[refactor-centiloid] No metric results returned." << std::endl;
        return;
    }
    std::cout << "\n=== Refactor Centiloid Results ===" << std::endl;
    for (const auto& metric : response.metricResults) {
        std::cout << "Metric: " << metric.metricName << std::endl;
        for (const auto& [tracer, value] : metric.tracerValues) {
            std::cout << "  " << tracer << ": " << value << std::endl;
        }
        if (includeSUVr) {
            std::cout << "  SUVr: " << metric.suvr << std::endl;
        }
    }
}

int runSingle(const CentiloidCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    refactorCommon::fs::ensureParentDirectory(options.outputPath);
    std::string debugBase =
        options.enableDebugOutput ? refactorCommon::path::deriveDebugBasePath(options.outputPath) : std::string{};
    ProcessingRequest request = buildProcessingRequest(
        options, options.inputPath, options.outputPath, debugBase);
    std::cout << "[refactor-centiloid] Starting processing: " << fullCommand << std::endl;
    try {
        auto response = app.run(request);
        std::cout << "\n[refactor-centiloid] Spatial normalization complete. Normalized image saved to "
                  << options.outputPath << std::endl;
        logMetricResults(response, options.includeSUVr);
    } catch (const std::exception& ex) {
        std::cerr << "[refactor-centiloid] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[refactor-centiloid] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

int runBatch(const CentiloidCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    const std::filesystem::path inputDir(options.inputPath);
    const std::filesystem::path outputDir(options.outputPath);

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "[refactor-centiloid] Input directory does not exist: " << options.inputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!refactorCommon::fs::ensureDirectory(outputDir)) {
        std::cerr << "[refactor-centiloid] Output path must be a directory: " << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!options.skipRegistration && !refactorCommon::fs::isDirectoryEmpty(outputDir)) {
        std::cerr << "[refactor-centiloid] Output directory must be empty unless --skip-normalization is set." << std::endl;
        return EXIT_FAILURE;
    }

    auto files = refactorCommon::fs::collectNiftiFiles(inputDir);
    if (files.empty()) {
        std::cout << "[refactor-centiloid] No NIfTI files found in " << inputDir << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[refactor-centiloid] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    BatchProcessingRequest batchRequest;
    batchRequest.items.reserve(files.size());
    for (const auto& path : files) {
        std::string outputPath = refactorCommon::fs::buildOutputPath(path, outputDir, "_centiloid_refactor.nii");
        std::string debugBase =
            options.enableDebugOutput ? refactorCommon::path::deriveDebugBasePath(outputPath) : std::string{};
        ProcessingRequest request = buildProcessingRequest(
            options, path.string(), outputPath, debugBase);
        BatchProcessingItem item;
        item.request = std::move(request);
        item.label = path.filename().string();
        batchRequest.items.push_back(std::move(item));
    }

    auto batchInfo = BatchLogging::openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    auto csvCtx = BatchLogging::openCsv(outputDir);

    auto onSuccess = [&](const BatchProcessingItem& item, const ProcessingResponse& response) {
        std::cout << "[refactor-centiloid][batch] Processed " << item.label << std::endl;
        logMetricResults(response, options.includeSUVr);
        BatchLogging::appendSuccessEntry(batchInfo, item.label);
        BatchLogging::appendCsvRows(csvCtx, item.label, response.metricResults);
    };

    auto onError = [&](const BatchProcessingItem& item, const std::exception& ex) {
        std::cerr << "[refactor-centiloid][batch] Failed " << item.label << ": " << ex.what() << std::endl;
        BatchLogging::appendFailureEntry(batchInfo, item.label, ex.what());
    };

    auto summary = app.runBatch(batchRequest, onSuccess, onError);
    BatchLogging::finalizeBatchInfo(batchInfo, summary);

    std::cout << "[refactor-centiloid] Batch complete. Success: "
              << summary.succeeded << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("centiloid")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<CentiloidLogic>(config));
}

int runCommand(const CentiloidCLIOptions& options, const std::string& fullCommand) {
    CentiloidCLIOptions optionsCopy = options;
    configureDebugOutputBasePath(optionsCopy);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "refactor-centiloid";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace RefactorPipeline::Metrics::Centiloid

