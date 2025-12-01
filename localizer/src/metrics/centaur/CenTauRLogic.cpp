#include "CenTauRLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/interfaces/IConfiguration.h"
#include "CenTauRCalculator.h"
#include "../shared/BatchMetricExecutor.h"
#include "../shared/DebugPathHelpers.h"
#include <iostream>
#include <stdexcept>

namespace Pipeline::Metrics::Centaur {

namespace {

using MetricHooks = Pipeline::Metrics::Shared::MetricExecutionHooks<CenTauRCLIOptions>;

class CenTauRLogic : public IMetricLogic {
public:
    explicit CenTauRLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("CenTauRLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "centaur";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        CenTauRCalculator calculator(config_);
        MetricResult result = calculator.calculate(request.spatiallyNormalizedImage);
        return {result};
    }

private:
    ConfigurationPtr config_;
};

ProcessingRequest buildProcessingRequest(const CenTauRCLIOptions& options,
                                         const std::string& inputPath,
                                         const std::string& outputPath,
                                         const std::string& debugBasePath) {
    ProcessingRequest request;
    request.outputPath = outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "centaur";

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
        std::cout << "[centaur] No metric results returned." << std::endl;
        return;
    }
    std::cout << "\n=== Refactor CenTauR Results ===" << std::endl;
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
    hooks.logTag = "centaur";
    hooks.batchOutputSuffix = "_centaur_refactor.nii";
    hooks.buildRequest = [](const CenTauRCLIOptions& options,
                            const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& debugBasePath) {
        return buildProcessingRequest(options, inputPath, outputPath, debugBasePath);
    };
    auto debugResolver = [](const CenTauRCLIOptions& options, const std::string& outputPath) {
        return options.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.resolveSingleDebug = debugResolver;
    hooks.resolveBatchDebug = debugResolver;
    hooks.logResults = [](const CenTauRCLIOptions& options, const ProcessingResponse& response) {
        logMetricResults(response, options.includeSUVr);
    };
    return hooks;
}

int runSingle(const CenTauRCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runSingleMetric(
        options, fullCommand, app, createExecutionHooks());
}

int runBatch(const CenTauRCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    return Pipeline::Metrics::Shared::runBatchMetric(
        options, fullCommand, app, createExecutionHooks());
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("centaur")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<CenTauRLogic>(config));
}

int runCommand(const CenTauRCLIOptions& options, const std::string& fullCommand) {
    CenTauRCLIOptions optionsCopy = options;
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "centaur";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace Pipeline::Metrics::Centaur


