#pragma once
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

#include "../../core/application/PipelineApplication.h"
#include "../../core/common/BatchLogging.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/config/Version.h"

namespace Pipeline::Metrics::Shared {

template <typename Options>
struct MetricExecutionHooks {
    using RequestBuilder =
        std::function<ProcessingRequest(const Options&, const std::string&, const std::string&, const std::string&)>;
    using DebugPathResolver = std::function<std::string(const Options&, const std::string&)>;
    using ResultLogger = std::function<void(const Options&, const ProcessingResponse&)>;

    std::string logTag;
    std::string batchOutputSuffix;
    RequestBuilder buildRequest;
    DebugPathResolver resolveSingleDebug;
    DebugPathResolver resolveBatchDebug;
    ResultLogger logResults;
};

template <typename Options>
int runSingleMetric(const Options& options,
                    const std::string& fullCommand,
                    PipelineApplication& app,
                    const MetricExecutionHooks<Options>& hooks) {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << hooks.logTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    std::string debugBase = hooks.resolveSingleDebug
        ? hooks.resolveSingleDebug(options, options.outputPath)
        : std::string{};

    ProcessingRequest request =
        hooks.buildRequest(options, options.inputPath, options.outputPath, debugBase);

    std::cout << "[" << hooks.logTag << "] Starting processing: " << fullCommand << std::endl;
    try {
        auto response = app.run(request);
        std::cout << "\n[" << hooks.logTag
                  << "] Spatial normalization complete. Normalized image saved to "
                  << options.outputPath << std::endl;
        hooks.logResults(options, response);
    } catch (const std::exception& ex) {
        std::cerr << "[" << hooks.logTag << "] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[" << hooks.logTag << "] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

template <typename Options>
int runBatchMetric(const Options& options,
                   const std::string& fullCommand,
                   PipelineApplication& app,
                   const MetricExecutionHooks<Options>& hooks) {
    const std::filesystem::path inputDir(options.inputPath);
    const std::filesystem::path outputDir(options.outputPath);

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "[" << hooks.logTag << "] Input directory does not exist: "
                  << options.inputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!Common::fs::ensureDirectory(outputDir)) {
        std::cerr << "[" << hooks.logTag << "] Output path must be a directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!options.skipRegistration && !Common::fs::isDirectoryEmpty(outputDir)) {
        std::cerr << "[" << hooks.logTag
                  << "] Output directory must be empty unless --skip-normalization is set."
                  << std::endl;
        return EXIT_FAILURE;
    }

    auto files = Common::fs::collectNiftiFiles(inputDir);
    if (files.empty()) {
        std::cout << "[" << hooks.logTag << "] No NIfTI files found in " << inputDir << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[" << hooks.logTag << "] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    BatchProcessingRequest batchRequest;
    batchRequest.items.reserve(files.size());
    for (const auto& path : files) {
        std::string outputPath =
            Common::fs::buildOutputPath(path, outputDir, hooks.batchOutputSuffix);
        std::string debugBase = hooks.resolveBatchDebug
            ? hooks.resolveBatchDebug(options, outputPath)
            : std::string{};
        BatchProcessingItem item;
        item.request = hooks.buildRequest(options, path.string(), outputPath, debugBase);
        item.label = path.filename().string();
        batchRequest.items.push_back(std::move(item));
    }

    auto batchInfo = BatchLogging::openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    auto csvCtx = BatchLogging::openCsv(outputDir);

    auto onSuccess = [&](const BatchProcessingItem& item, const ProcessingResponse& response) {
        std::cout << "[" << hooks.logTag << "][batch] Processed " << item.label << std::endl;
        hooks.logResults(options, response);
        BatchLogging::appendSuccessEntry(batchInfo, item.label);
        BatchLogging::appendCsvRows(csvCtx, item.label, response.metricResults);
    };

    auto onError = [&](const BatchProcessingItem& item, const std::exception& ex) {
        std::cerr << "[" << hooks.logTag << "][batch] Failed " << item.label
                  << ": " << ex.what() << std::endl;
        BatchLogging::appendFailureEntry(batchInfo, item.label, ex.what());
    };

    auto summary = app.runBatch(batchRequest, onSuccess, onError);
    BatchLogging::finalizeBatchInfo(batchInfo, summary);

    std::cout << "[" << hooks.logTag << "] Batch complete. Success: "
              << summary.succeeded << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace Pipeline::Metrics::Shared
