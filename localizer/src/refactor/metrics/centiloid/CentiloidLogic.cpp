#include "CentiloidLogic.h"
#include "../../interfaces/IMetricModuleRegistry.h"
#include "../../common/ProcessingContracts.h"
#include "../../di/Bootstrap.h"
#include "../../../calculators/CentiloidCalculator.h"
#include "../../../config/Configuration.h"
#include "../../../interfaces/IConfiguration.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace RefactorPipeline::Metrics::Centiloid {

namespace {

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

ConfigurationPtr loadConfiguration(const std::string& configPath, bool debug) {
    auto configuration = std::make_shared<Configuration>();
    std::string resolvedPath = configPath.empty() ? "config.toml" : Configuration::findConfigFile(configPath);
    if (!configuration->loadFromFile(resolvedPath)) {
        std::cerr << "[refactor-centiloid] Failed to load configuration from " << resolvedPath << std::endl;
    } else {
        std::cout << "[refactor-centiloid] Loaded configuration from " << resolvedPath << std::endl;
        if (debug) {
            configuration->printAllConfigurations();
        }
    }
    return configuration;
}

void ensureOutputDirectoryExists(const std::string& outputPath) {
    auto directory = std::filesystem::path(outputPath).parent_path();
    if (!directory.empty() && !std::filesystem::exists(directory)) {
        std::filesystem::create_directories(directory);
    }
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

int runCommand(const SUVrDerivedMetricOptions& options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[refactor-centiloid] Batch mode is not supported in the prototype refactor path yet." << std::endl;
        return EXIT_FAILURE;
    }

    SUVrDerivedMetricOptions optionsCopy = options;
    setupDebugOutput(optionsCopy);

    auto config = loadConfiguration(optionsCopy.configPath, optionsCopy.enableDebugOutput);
    if (!config) {
        return EXIT_FAILURE;
    }

    ensureOutputDirectoryExists(optionsCopy.outputPath);

    auto container = buildDefaultContainer(config);
    registerMetric(*container);

    ProcessingRequest request;
    request.outputPath = optionsCopy.outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "centiloid";

    request.normalization.inputPath = optionsCopy.inputPath;
    request.normalization.skip = optionsCopy.skipRegistration;
    request.normalization.options.useIterativeRigid = optionsCopy.useIterativeRigid;
    request.normalization.options.useManualFOV = optionsCopy.useManualFOV;
    request.normalization.options.enableDebugOutput = optionsCopy.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = optionsCopy.debugOutputBasePath;

    std::cout << "[refactor-centiloid] Starting processing: " << fullCommand << std::endl;
    auto app = resolveApplication(*container);

    try {
        auto response = app->run(request);
        std::cout << "\n[refactor-centiloid] Spatial normalization complete. Normalized image saved to "
                  << optionsCopy.outputPath << std::endl;
        if (response.metricResults.empty()) {
            std::cout << "[refactor-centiloid] No metric results returned." << std::endl;
        } else {
            std::cout << "\n=== Refactor Centiloid Results ===" << std::endl;
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
        std::cerr << "[refactor-centiloid] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[refactor-centiloid] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace RefactorPipeline::Metrics::Centiloid

