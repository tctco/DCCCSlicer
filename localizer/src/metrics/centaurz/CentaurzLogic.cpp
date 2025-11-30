#include "CentaurzLogic.h"
#include "../../core/interfaces/IMetricLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "CenTauRzCalculator.h"
#include "../../config/Configuration.h"
#include "../../interfaces/IConfiguration.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace RefactorPipeline::Metrics::Centaurz {

namespace {

void configureDebugOutputBasePath(CentaurzCLIOptions& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        return;
    }
    std::filesystem::path outputFilePath(options.outputPath);
    std::string directory = outputFilePath.parent_path().string();
    std::string baseName = outputFilePath.stem().string();
    if (directory.empty()) {
        options.debugOutputBasePath = baseName;
    } else {
        options.debugOutputBasePath = directory + "/" + baseName;
    }
}

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

ConfigurationPtr loadConfiguration(const std::string& configPath, bool debug) {
    auto configuration = std::make_shared<Configuration>();
    std::string resolvedPath = configPath.empty() ? "config.toml" : Configuration::findConfigFile(configPath);
    if (!configuration->loadFromFile(resolvedPath)) {
        std::cerr << "[refactor-centaurz] Failed to load configuration from " << resolvedPath << std::endl;
    } else {
        std::cout << "[refactor-centaurz] Loaded configuration from " << resolvedPath << std::endl;
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
    if (registry->hasModule("centaurz")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<CentaurzLogic>(config));
}

int runCommand(const CentaurzCLIOptions& options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[refactor-centaurz] Batch mode is not supported in the prototype refactor path yet." << std::endl;
        return EXIT_FAILURE;
    }

    CentaurzCLIOptions optionsCopy = options;
    configureDebugOutputBasePath(optionsCopy);

    auto config = loadConfiguration(optionsCopy.configPath, optionsCopy.enableDebugOutput);
    if (!config) {
        return EXIT_FAILURE;
    }

    ensureOutputDirectoryExists(optionsCopy.outputPath);

    auto container = buildDefaultContainer(config);

    ProcessingRequest request;
    request.outputPath = optionsCopy.outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "centaurz";

    request.normalization.inputPath = optionsCopy.inputPath;
    request.normalization.skip = optionsCopy.skipRegistration;
    request.normalization.options.useIterativeRigid = optionsCopy.useIterativeRigid;
    request.normalization.options.useManualFOV = optionsCopy.useManualFOV;
    request.normalization.options.enableDebugOutput = optionsCopy.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = optionsCopy.debugOutputBasePath;

    std::cout << "[refactor-centaurz] Starting processing: " << fullCommand << std::endl;
    auto app = resolveApplication(*container);

    try {
        auto response = app->run(request);
        std::cout << "\n[refactor-centaurz] Spatial normalization complete. Normalized image saved to "
                  << optionsCopy.outputPath << std::endl;
        if (response.metricResults.empty()) {
            std::cout << "[refactor-centaurz] No metric results returned." << std::endl;
        } else {
            std::cout << "\n=== Refactor CenTauRz Results ===" << std::endl;
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
        std::cerr << "[refactor-centaurz] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[refactor-centaurz] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace RefactorPipeline::Metrics::Centaurz


