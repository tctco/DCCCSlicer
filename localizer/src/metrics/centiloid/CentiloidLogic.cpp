#include "CentiloidLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "CentiloidCalculator.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../shared/BatchMetricExecutor.h"
#include "../shared/DebugPathHelpers.h"
#include <iostream>
#include <stdexcept>

namespace Pipeline::Metrics::Centiloid {

namespace {

using MetricHooks = Pipeline::Metrics::Shared::MetricExecutionHooks<CentiloidCLIOptions>;

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
        std::cout << "[centiloid] No metric results returned." << std::endl;
        return;
    }
    std::cout << "\n=== Centiloid Results ===" << std::endl;
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
    hooks.logTag = "centiloid";
    hooks.batchOutputSuffix = "_centiloid.nii";
    hooks.buildRequest = [](const CentiloidCLIOptions& options,
                            const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& debugBasePath) {
        return buildProcessingRequest(options, inputPath, outputPath, debugBasePath);
    };
    auto debugResolver = [](const CentiloidCLIOptions& options, const std::string& outputPath) {
        return options.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.resolveSingleDebug = debugResolver;
    hooks.resolveBatchDebug = debugResolver;
    hooks.logResults = [](const CentiloidCLIOptions& options, const ProcessingResponse& response) {
        logMetricResults(response, options.includeSUVr);
    };
    return hooks;
}

int runSingle(const CentiloidCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runSingleMetric(
        options, fullCommand, app, createExecutionHooks());
}

int runBatch(const CentiloidCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runBatchMetric(
        options, fullCommand, app, createExecutionHooks());
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
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "centiloid";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace Pipeline::Metrics::Centiloid

