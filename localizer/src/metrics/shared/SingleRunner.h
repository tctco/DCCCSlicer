#pragma once

#include "MetricRunResult.h"
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>

namespace Pipeline::Metrics::Shared {

template <typename Options>
struct SingleRunnerHooks {
    using Execute = std::function<MetricRunResult(
        const Options& options,
        const std::string& inputPath,
        const std::string& outputPath,
        const std::string& debugBasePath)>;
    using DebugPathResolver =
        std::function<std::string(const Options& options, const std::string& outputPath)>;
    using ResultLogger = std::function<void(const Options& options, const MetricRunResult& result)>;

    std::string logTag;
    Execute execute;
    DebugPathResolver resolveDebugBase;
    ResultLogger logResults;
};

template <typename Options>
int runSingle(const Options& options,
              const std::string& fullCommand,
              const SingleRunnerHooks<Options>& hooks) {
    const std::string debugBase = hooks.resolveDebugBase
        ? hooks.resolveDebugBase(options, options.outputPath)
        : std::string{};

    std::cout << "[" << hooks.logTag << "] Starting processing: " << fullCommand << std::endl;
    try {
        MetricRunResult result =
            hooks.execute(options, options.inputPath, options.outputPath, debugBase);
        if (hooks.logResults) {
            hooks.logResults(options, result);
        }
    } catch (const std::exception& ex) {
        std::cerr << "[" << hooks.logTag << "] Processing failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[" << hooks.logTag << "] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace Pipeline::Metrics::Shared
