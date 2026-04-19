#pragma once

#include "BatchLogging.h"
#include "MetricRunResult.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/Version.h"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace Pipeline::Metrics::Shared {

template <typename Options>
struct BatchRunnerHooks {
    using Execute = std::function<MetricRunResult(
        const Options& options,
        const std::string& inputPath,
        const std::string& outputPath,
        const std::string& debugBasePath)>;
    using DebugPathResolver =
        std::function<std::string(const Options& options, const std::string& outputPath)>;
    using ResultLogger = std::function<void(const Options& options, const MetricRunResult& result)>;

    std::string logTag;
    std::string batchOutputSuffix;
    Execute execute;
    DebugPathResolver resolveDebugBase;
    ResultLogger logResults;
};

template <typename Options>
int runBatch(const Options& options,
             const std::string& fullCommand,
             const BatchRunnerHooks<Options>& hooks) {
    const std::filesystem::path inputDir = Common::path::fromUtf8(options.inputPath);
    const std::filesystem::path outputDir = Common::path::fromUtf8(options.outputPath);

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

    const auto files = Common::fs::collectNiftiFiles(inputDir);
    if (files.empty()) {
        std::cout << "[" << hooks.logTag << "] No NIfTI files found in "
                  << Common::path::toUtf8(inputDir) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[" << hooks.logTag << "] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    auto batchInfo = openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    auto csvCtx = openCsv(outputDir);
    BatchSummary summary;

    for (const auto& inputFile : files) {
        summary.processed++;
        const std::string outputPath =
            Common::fs::buildOutputPath(inputFile, outputDir, hooks.batchOutputSuffix);
        const std::string inputPath = Common::path::toUtf8(inputFile);
        const std::string fileLabel = Common::path::toUtf8(inputFile.filename());
        const std::string debugBase = hooks.resolveDebugBase
            ? hooks.resolveDebugBase(options, outputPath)
            : std::string{};

        try {
            MetricRunResult result =
                hooks.execute(options, inputPath, outputPath, debugBase);
            summary.succeeded++;
            std::cout << "[" << hooks.logTag << "][batch] Processed "
                      << fileLabel << std::endl;
            if (hooks.logResults) {
                hooks.logResults(options, result);
            }
            appendSuccessEntry(batchInfo, fileLabel);
            appendCsvRows(csvCtx, fileLabel, result.metricResults);
        } catch (const std::exception& ex) {
            summary.failed++;
            std::cerr << "[" << hooks.logTag << "][batch] Failed "
                      << fileLabel << ": " << ex.what() << std::endl;
            appendFailureEntry(batchInfo, fileLabel, ex.what());
        }
    }

    finalizeBatchInfo(batchInfo, summary);
    std::cout << "[" << hooks.logTag << "] Batch complete. Success: "
              << summary.succeeded << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace Pipeline::Metrics::Shared
