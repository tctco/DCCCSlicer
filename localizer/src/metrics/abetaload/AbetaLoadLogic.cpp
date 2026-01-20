#include "AbetaLoadLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/interfaces/IConfiguration.h"
#include "../shared/BatchMetricExecutor.h"
#include "../shared/DebugPathHelpers.h"
#include "AbetaLoadCalculator.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::AbetaLoad {

namespace {

using MetricHooks = Pipeline::Metrics::Shared::MetricExecutionHooks<AbetaLoadCLIOptions>;

class AbetaLoadLogic : public IMetricLogic {
public:
    explicit AbetaLoadLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("AbetaLoadLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "abetaload";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        AbetaLoadCalculator calculator(config_);
        MetricResult result = calculator.calculate(request.spatiallyNormalizedImage);
        return {result};
    }

private:
    ConfigurationPtr config_;
};

ProcessingRequest buildProcessingRequest(const AbetaLoadCLIOptions& options,
                                         const std::string& inputPath,
                                         const std::string& outputPath,
                                         const std::string& debugBasePath) {
    ProcessingRequest request;
    request.outputPath = outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "abetaload";

    request.normalization.inputPath = inputPath;
    request.normalization.skip = options.skipRegistration;
    request.normalization.options.useIterativeRigid = options.useIterativeRigid;
    request.normalization.options.useManualFOV = options.useManualFOV;
    request.normalization.options.enableDebugOutput = options.enableDebugOutput;
    request.normalization.options.debugOutputBasePath =
        options.enableDebugOutput ? debugBasePath : std::string{};
    return request;
}

void logMetricResults(const ProcessingResponse& response) {
    if (response.metricResults.empty()) {
        std::cout << "[abetaload] No metric results returned." << std::endl;
        return;
    }
    std::cout << "\n=== AbetaLoad Results ===" << std::endl;
    for (const auto& metric : response.metricResults) {
        std::cout << "Metric: " << metric.metricName << std::endl;
        for (const auto& [label, value] : metric.tracerValues) {
            std::cout << "  " << label << ": " << value << std::endl;
        }
        std::cout << "  Abeta_load (SUVr field): " << metric.suvr << std::endl;
    }
}

MetricHooks createExecutionHooks() {
    MetricHooks hooks;
    hooks.logTag = "abetaload";
    hooks.batchOutputSuffix = "_abetaload.nii";
    hooks.buildRequest = [](const AbetaLoadCLIOptions& options,
                            const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& debugBasePath) {
        return buildProcessingRequest(options, inputPath, outputPath, debugBasePath);
    };
    auto debugResolver = [](const AbetaLoadCLIOptions& options, const std::string& outputPath) {
        return options.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.resolveSingleDebug = debugResolver;
    hooks.resolveBatchDebug = debugResolver;
    hooks.logResults = [](const AbetaLoadCLIOptions&, const ProcessingResponse& response) {
        logMetricResults(response);
    };
    return hooks;
}

int runSingle(const AbetaLoadCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runSingleMetric(
        options, fullCommand, app, createExecutionHooks());
}

int runBatch(const AbetaLoadCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runBatchMetric(
        options, fullCommand, app, createExecutionHooks());
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("abetaload")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<AbetaLoadLogic>(config));
}

int runCommand(const AbetaLoadCLIOptions& options, const std::string& fullCommand) {
    AbetaLoadCLIOptions optionsCopy = options;
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "abetaload";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace Pipeline::Metrics::AbetaLoad

