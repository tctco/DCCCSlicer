#include "CentaurzLogic.h"
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/PathUtils.h"
#include "../../core/common/BatchLogging.h"
#include "../../core/config/Version.h"
#include "../../core/di/Bootstrap.h"
#include "CenTauRzCalculator.h"
#include "../../core/interfaces/IConfiguration.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace Pipeline::Metrics::Centaurz {

namespace {

void configureDebugOutputBasePath(CentaurzCLIOptions& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        return;
    }
    std::filesystem::path outputFilePath(options.outputPath);
    std::string directory = outputFilePath.parent_path().string();
    std::string baseName = outputFilePath.stem().string();
    if (directory.empty()) {
        options.debugOutputBasePath = baseName;
    } else {
        options.debugOutputBasePath = directory + "/" + baseName;
    }
}

class CentaurzLogic : public IMetricLogic {
public:
    explicit CentaurzLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("CentaurzLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "centaurz";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        CenTauRzCalculator calculator(config_);
        MetricResult result = calculator.calculate(request.spatiallyNormalizedImage);
        return {result};
    }

private:
    ConfigurationPtr config_;
};

ProcessingRequest buildProcessingRequest(const CentaurzCLIOptions& options,
                                         const std::string& inputPath,
                                         const std::string& outputPath,
                                         const std::string& debugBasePath) {
    ProcessingRequest request;
    request.outputPath = outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "centaurz";

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
        std::cout << "[centaurz] No metric results returned." << std::endl;
        return;
    }

    std::cout << "\n=== Refactor CenTauRz Results ===" << std::endl;
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

int runSingle(const CentaurzCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[centaurz] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    std::string debugBase =
        options.enableDebugOutput ? options.debugOutputBasePath : std::string{};
    ProcessingRequest request = buildProcessingRequest(
        options, options.inputPath, options.outputPath, debugBase);

    std::cout << "[centaurz] Starting processing: " << fullCommand << std::endl;
    try {
        auto response = app.run(request);
        std::cout << "\n[centaurz] Spatial normalization complete. Normalized image saved to "
                  << options.outputPath << std::endl;
        logMetricResults(response, options.includeSUVr);
    } catch (const std::exception& ex) {
        std::cerr << "[centaurz] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[centaurz] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

int runBatch(const CentaurzCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    const std::filesystem::path inputDir(options.inputPath);
    const std::filesystem::path outputDir(options.outputPath);

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "[centaurz] Input directory does not exist: "
                  << options.inputPath << std::endl;
        return EXIT_FAILURE;
    }

    if (!Common::fs::ensureDirectory(outputDir)) {
        std::cerr << "[centaurz] Output path must be a directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    if (!options.skipRegistration && !Common::fs::isDirectoryEmpty(outputDir)) {
        std::cerr << "[centaurz] Output directory must be empty unless --skip-normalization is set."
                  << std::endl;
        return EXIT_FAILURE;
    }

    auto files = Common::fs::collectNiftiFiles(inputDir);
    if (files.empty()) {
        std::cout << "[centaurz] No NIfTI files found in " << inputDir << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[centaurz] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    BatchProcessingRequest batchRequest;
    batchRequest.items.reserve(files.size());

    for (const auto& path : files) {
        std::string outputPath =
            Common::fs::buildOutputPath(path, outputDir, "_centaurz_refactor.nii");
        std::string debugBase = options.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};

        BatchProcessingItem item;
        item.request = buildProcessingRequest(options, path.string(), outputPath, debugBase);
        item.label = path.filename().string();
        batchRequest.items.push_back(std::move(item));
    }

    auto batchInfo = BatchLogging::openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    auto csvCtx = BatchLogging::openCsv(outputDir);

    auto onSuccess = [&](const BatchProcessingItem& item, const ProcessingResponse& response) {
        std::cout << "[centaurz][batch] Processed " << item.label << std::endl;
        logMetricResults(response, options.includeSUVr);
        BatchLogging::appendSuccessEntry(batchInfo, item.label);
        BatchLogging::appendCsvRows(csvCtx, item.label, response.metricResults);
    };

    auto onError = [&](const BatchProcessingItem& item, const std::exception& ex) {
        std::cerr << "[centaurz][batch] Failed " << item.label << ": "
                  << ex.what() << std::endl;
        BatchLogging::appendFailureEntry(batchInfo, item.label, ex.what());
    };

    auto summary = app.runBatch(batchRequest, onSuccess, onError);
    BatchLogging::finalizeBatchInfo(batchInfo, summary);

    std::cout << "[centaurz] Batch complete. Success: "
              << summary.succeeded << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("centaurz")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<CentaurzLogic>(config));
}

int runCommand(const CentaurzCLIOptions& options, const std::string& fullCommand) {
    CentaurzCLIOptions optionsCopy = options;
    if (!optionsCopy.batchMode) {
        configureDebugOutputBasePath(optionsCopy);
    }

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "centaurz";

    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace Pipeline::Metrics::Centaurz


