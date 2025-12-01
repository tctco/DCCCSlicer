#include "CentaurzLogic.h"
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/common/PathUtils.h"
#include "../../core/di/Bootstrap.h"
#include "CenTauRzCalculator.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../shared/BatchMetricExecutor.h"
#include "../shared/DebugPathHelpers.h"
#include <iostream>
#include <stdexcept>

namespace Pipeline::Metrics::Centaurz {

namespace {

using MetricHooks = Pipeline::Metrics::Shared::MetricExecutionHooks<CentaurzCLIOptions>;

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

MetricHooks createExecutionHooks() {
    MetricHooks hooks;
    hooks.logTag = "centaurz";
    hooks.batchOutputSuffix = "_centaurz_refactor.nii";
    hooks.buildRequest = [](const CentaurzCLIOptions& options,
                            const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& debugBasePath) {
        return buildProcessingRequest(options, inputPath, outputPath, debugBasePath);
    };
    hooks.resolveSingleDebug = [](const CentaurzCLIOptions& options, const std::string&) {
        return options.enableDebugOutput ? options.debugOutputBasePath : std::string{};
    };
    hooks.resolveBatchDebug = [](const CentaurzCLIOptions& options, const std::string& outputPath) {
        return options.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.logResults = [](const CentaurzCLIOptions& options, const ProcessingResponse& response) {
        logMetricResults(response, options.includeSUVr);
    };
    return hooks;
}

int runSingle(const CentaurzCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runSingleMetric(
        options, fullCommand, app, createExecutionHooks());
}

int runBatch(const CentaurzCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runBatchMetric(
        options, fullCommand, app, createExecutionHooks());
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
        Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);
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


