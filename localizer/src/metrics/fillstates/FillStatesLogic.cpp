#include "FillStatesLogic.h"
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "FillStatesCalculator.h"
#include "../../core/interfaces/IConfiguration.h"
#include <iostream>
#include <stdexcept>

namespace RefactorPipeline::Metrics::FillStates {

namespace {

constexpr const char* kTracerParam = "fillstates_tracer";
constexpr const char* kMaskOutputParam = "fillstates_mask_output";

void configureDebugOutputBasePath(FillStatesCLIOptions& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        return;
    }
    options.debugOutputBasePath = refactorCommon::path::deriveDebugBasePath(options.outputPath);
}

class FillStatesLogic : public IMetricLogic {
public:
    explicit FillStatesLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("FillStatesLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "fillstates";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        if (!request.spatiallyNormalizedImage) {
            throw std::invalid_argument("FillStates metric requires a spatially normalized image");
        }

        const std::string tracer = resolveTracer(request.options);
        FillStatesCalculator calculator(config_);
        calculator.setTracer(tracer);

        MetricResult result = calculator.calculate(request.spatiallyNormalizedImage);
        saveMaskIfRequested(calculator.getLastMaskImage(), request.options);
        return {result};
    }

private:
    static std::string resolveTracer(const MetricComputationOptions& options) {
        auto it = options.stringParameters.find(kTracerParam);
        if (it == options.stringParameters.end() || it->second.empty()) {
            throw std::invalid_argument("FillStates metric requires a tracer parameter");
        }
        return it->second;
    }

    void saveMaskIfRequested(ImageType::Pointer mask, const MetricComputationOptions& options) const {
        if (!mask) {
            return;
        }
        auto it = options.stringParameters.find(kMaskOutputParam);
        if (it == options.stringParameters.end() || it->second.empty()) {
            return;
        }
        const std::string& maskPath = it->second;
        try {
            refactorCommon::fs::ensureParentDirectory(maskPath);
            refactorCommon::nifti::saveImage(mask, maskPath);
            std::cout << "[refactor-fillstates] Fill-states mask saved to " << maskPath << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[refactor-fillstates] Failed to save fill-states mask: " << ex.what() << std::endl;
        }
    }

    ConfigurationPtr config_;
};

void ensureOutputDirectoryExists(const std::string& outputPath) {
    refactorCommon::fs::ensureParentDirectory(outputPath);
}

std::string buildMaskOutputPath(const std::string& outputPath) {
    if (outputPath.empty()) {
        return {};
    }
    return refactorCommon::path::addSuffix(outputPath, "_fill_states_map");
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("fillstates")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<FillStatesLogic>(config));
}

int runCommand(const FillStatesCLIOptions& options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[refactor-fillstates] Batch mode is not supported in the prototype refactor path yet." << std::endl;
        return EXIT_FAILURE;
    }

    if (options.tracer.empty()) {
        std::cerr << "[refactor-fillstates] --tracer is required." << std::endl;
        return EXIT_FAILURE;
    }

    FillStatesCLIOptions optionsCopy = options;
    configureDebugOutputBasePath(optionsCopy);

    ensureOutputDirectoryExists(optionsCopy.outputPath);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "refactor-fillstates";

    auto container = buildDefaultContainer(bootstrapOptions);

    ProcessingRequest request;
    request.outputPath = optionsCopy.outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "fillstates";
    request.metricOptions.stringParameters[kTracerParam] = optionsCopy.tracer;
    std::string maskOutputPath = buildMaskOutputPath(optionsCopy.outputPath);
    if (!maskOutputPath.empty()) {
        request.metricOptions.stringParameters[kMaskOutputParam] = maskOutputPath;
    }

    request.normalization.inputPath = optionsCopy.inputPath;
    request.normalization.skip = optionsCopy.skipRegistration;
    request.normalization.options.useIterativeRigid = optionsCopy.useIterativeRigid;
    request.normalization.options.useManualFOV = optionsCopy.useManualFOV;
    request.normalization.options.enableDebugOutput = optionsCopy.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = optionsCopy.debugOutputBasePath;

    std::cout << "[refactor-fillstates] Starting processing: " << fullCommand << std::endl;
    auto app = resolveApplication(*container);

    try {
        auto response = app->run(request);
        std::cout << "\n[refactor-fillstates] Spatial normalization complete. Normalized image saved to "
                  << optionsCopy.outputPath << std::endl;
        if (response.metricResults.empty()) {
            std::cout << "[refactor-fillstates] No metric results returned." << std::endl;
        } else {
            std::cout << "\n=== Refactor FillStates Results ===" << std::endl;
            for (const auto& metric : response.metricResults) {
                std::cout << "Metric: " << metric.metricName << std::endl;
                for (const auto& [tracer, value] : metric.tracerValues) {
                    std::cout << "  " << tracer << ": " << value << std::endl;
                }
                if (optionsCopy.includeSUVr) {
                    std::cout << "  SUVr: " << metric.suvr << std::endl;
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[refactor-fillstates] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[refactor-fillstates] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace RefactorPipeline::Metrics::FillStates


