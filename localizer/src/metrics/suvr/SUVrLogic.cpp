#include "SUVrLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/interfaces/IConfiguration.h"
#include "SUVrCalculator.h"
#include "../shared/BatchMetricExecutor.h"
#include "../shared/DebugPathHelpers.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Pipeline::Metrics::SUVr {

namespace {

constexpr const char* kVoiMaskParam = "voi_mask_path";
constexpr const char* kRefMaskParam = "ref_mask_path";

using MetricHooks = Pipeline::Metrics::Shared::MetricExecutionHooks<SUVrCLIOptions>;

class SUVrLogic : public IMetricLogic {
public:
    explicit SUVrLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("SUVrLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "suvr";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        std::string voiMaskPath = getParameter(request.options, kVoiMaskParam, config_->getMaskPath("centiloid_voi"));
        std::string refMaskPath = getParameter(request.options, kRefMaskParam, config_->getMaskPath("whole_cerebral"));

        if (voiMaskPath.empty() || refMaskPath.empty()) {
            throw std::runtime_error("SUVr metric requires VOI and reference mask paths");
        }

        double suvr = SUVrCalculator::calculateSUVr(request.spatiallyNormalizedImage, voiMaskPath, refMaskPath);

        MetricResult metric;
        metric.metricName = "SUVr";
        metric.suvr = suvr;
        metric.tracerValues["SUVr"] = static_cast<float>(suvr);
        return {metric};
    }

private:
    static std::string getParameter(const MetricComputationOptions& options,
                                    const std::string& key,
                                    const std::string& fallback) {
        auto it = options.stringParameters.find(key);
        if (it != options.stringParameters.end() && !it->second.empty()) {
            return it->second;
        }
        return fallback;
    }

    ConfigurationPtr config_;
};

ProcessingRequest buildProcessingRequest(const SUVrCLIOptions& options,
                                         const std::string& inputPath,
                                         const std::string& outputPath,
                                         const std::string& debugBasePath) {
    ProcessingRequest request;
    request.outputPath = outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "suvr";

    if (!options.voiMaskPath.empty()) {
        request.metricOptions.stringParameters[kVoiMaskParam] = options.voiMaskPath;
    }
    if (!options.refMaskPath.empty()) {
        request.metricOptions.stringParameters[kRefMaskParam] = options.refMaskPath;
    }

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
        std::cout << "[suvr] Metric calculation skipped or returned no results." << std::endl;
        return;
    }

    std::cout << "\n=== Refactor SUVr Metrics ===" << std::endl;
    for (const auto& metric : response.metricResults) {
        std::cout << metric.metricName << ": " << metric.suvr << std::endl;
    }
}

MetricHooks createExecutionHooks() {
    MetricHooks hooks;
    hooks.logTag = "suvr";
    hooks.batchOutputSuffix = "_suvr.nii";
    hooks.buildRequest = [](const SUVrCLIOptions& options,
                            const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& debugBasePath) {
        return buildProcessingRequest(options, inputPath, outputPath, debugBasePath);
    };
    auto singleResolver = [](const SUVrCLIOptions& options, const std::string& outputPath) {
        return options.enableDebugOutput ? Common::path::deriveDebugBasePath(outputPath) : std::string{};
    };
    hooks.resolveSingleDebug = singleResolver;
    hooks.resolveBatchDebug = singleResolver;
    hooks.logResults = [](const SUVrCLIOptions&, const ProcessingResponse& response) {
        logMetricResults(response);
    };
    return hooks;
}

int runSingle(const SUVrCLIOptions& options,
              const std::string& fullCommand,
              PipelineApplication& app) {
    Common::path::requireOutputDirectoryExists(options.outputPath);
    return Pipeline::Metrics::Shared::runSingleMetric(
        options, fullCommand, app, createExecutionHooks());
}

int runBatch(const SUVrCLIOptions& options,
             const std::string& fullCommand,
             PipelineApplication& app) {
    const std::filesystem::path outputDir(options.outputPath);
    if (!std::filesystem::exists(outputDir) || !std::filesystem::is_directory(outputDir)) {
        std::cerr << "[suvr] Output path must be an existing directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }
    return Pipeline::Metrics::Shared::runBatchMetric(
        options, fullCommand, app, createExecutionHooks());
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("suvr")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<SUVrLogic>(config));
}

int runCommand(const SUVrCLIOptions& options, const std::string& fullCommand) {
    if (options.voiMaskPath.empty() || options.refMaskPath.empty()) {
        std::cerr << "[suvr] Both --voi-mask and --ref-mask must be provided for the prototype." << std::endl;
        return EXIT_FAILURE;
    }

    SUVrCLIOptions optionsCopy = options;
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "suvr";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto app = resolveApplication(*container);

    if (optionsCopy.batchMode) {
        return runBatch(optionsCopy, fullCommand, *app);
    }

    return runSingle(optionsCopy, fullCommand, *app);
}

} // namespace Pipeline::Metrics::SUVr

