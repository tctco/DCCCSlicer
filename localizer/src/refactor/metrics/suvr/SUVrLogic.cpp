#include "SUVrLogic.h"
#include "../../interfaces/IMetricModuleRegistry.h"
#include "../../common/ProcessingContracts.h"
#include "../../di/Bootstrap.h"
#include "../../../config/Configuration.h"
#include "../../../interfaces/IConfiguration.h"
#include "../../../calculators/SUVrCalculator.h"
#include "../../../utils/common.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace RefactorPipeline::Metrics::SUVr {

namespace {

constexpr const char* kVoiMaskParam = "voi_mask_path";
constexpr const char* kRefMaskParam = "ref_mask_path";

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

ConfigurationPtr loadConfiguration(const std::string& configPath, bool debug) {
    auto configuration = std::make_shared<Configuration>();
    std::string resolvedPath = configPath.empty() ? "config.toml" : Configuration::findConfigFile(configPath);
    if (!configuration->loadFromFile(resolvedPath)) {
        std::cerr << "[refactor-suvr] Failed to load configuration from " << resolvedPath << std::endl;
    } else {
        std::cout << "[refactor-suvr] Loaded configuration from " << resolvedPath << std::endl;
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
    if (registry->hasModule("suvr")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<SUVrLogic>(config));
}

int runCommand(const SUVrCommandOptions& options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[refactor-suvr] Batch mode is not supported in the prototype refactor path yet." << std::endl;
        return EXIT_FAILURE;
    }

    if (options.voiMaskPath.empty() || options.refMaskPath.empty()) {
        std::cerr << "[refactor-suvr] Both --voi-mask and --ref-mask must be provided for the prototype." << std::endl;
        return EXIT_FAILURE;
    }

    SUVrCommandOptions optionsCopy = options;
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
    request.metricOptions.metricName = "suvr";
    if (!optionsCopy.voiMaskPath.empty()) {
        request.metricOptions.stringParameters["voi_mask_path"] = optionsCopy.voiMaskPath;
    }
    if (!optionsCopy.refMaskPath.empty()) {
        request.metricOptions.stringParameters["ref_mask_path"] = optionsCopy.refMaskPath;
    }

    request.normalization.inputPath = optionsCopy.inputPath;
    request.normalization.skip = optionsCopy.skipRegistration;
    request.normalization.options.useIterativeRigid = optionsCopy.useIterativeRigid;
    request.normalization.options.useManualFOV = optionsCopy.useManualFOV;
    request.normalization.options.enableDebugOutput = optionsCopy.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = optionsCopy.debugOutputBasePath;

    std::cout << "[refactor-suvr] Starting processing: " << fullCommand << std::endl;
    auto app = resolveApplication(*container);

    try {
        auto response = app->run(request);
        std::cout << "\n[refactor-suvr] Spatial normalization complete. Normalized image saved to "
                  << optionsCopy.outputPath << std::endl;
        if (!response.metricResults.empty()) {
            std::cout << "\n=== Refactor SUVr Metrics ===" << std::endl;
            for (const auto& metric : response.metricResults) {
                std::cout << metric.metricName << ": " << metric.suvr << std::endl;
            }
        } else {
            std::cout << "[refactor-suvr] Metric calculation skipped or returned no results." << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "[refactor-suvr] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[refactor-suvr] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace RefactorPipeline::Metrics::SUVr

