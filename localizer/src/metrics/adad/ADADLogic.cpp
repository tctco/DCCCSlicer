#include "ADADLogic.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/interfaces/IConfiguration.h"
#include "Decoupler.h"
#include "../shared/DebugPathHelpers.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::ADAD {

namespace {

constexpr const char* kModalityParam = "modality";
constexpr const char* kOutputPathParam = "output_path";

std::string normalizeModality(const std::string& raw) {
    std::string modality = Common::path::toLower(raw);
    if (modality != "tau") {
        modality = "abeta";
    }
    return modality;
}

class ADADLogic : public IMetricLogic {
public:
    explicit ADADLogic(ConfigurationPtr config)
        : config_(std::move(config)) {
        if (!config_) {
            throw std::invalid_argument("ADADLogic requires configuration");
        }
    }

    std::string getName() const override {
        return "adad";
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest& request) override {
        if (!request.spatiallyNormalizedImage) {
            throw std::invalid_argument("ADAD metric requires a spatially normalized image");
        }
        if (!request.rigidAlignedImage) {
            throw std::invalid_argument("ADAD metric requires a rigid-aligned image");
        }

        const std::string modality = resolveModality(request.options);
        ImageType::Pointer adniImage = prepareADNIStyleImage(
            request.rigidAlignedImage,
            request.spatiallyNormalizedImage);
        DecoupledResult decoupled = runDecoupler(modality, adniImage);
        saveDecouplingOutputs(decoupled, request.options);
        MetricResult metric;
        metric.metricName = modality == "tau" ? "ADAD_tau" : "ADAD_abeta";
        metric.suvr = decoupled.ADADscore;
        metric.tracerValues = buildTracerValues(modality, decoupled);
        return {metric};
    }

private:
    ImageType::Pointer prepareADNIStyleImage(ImageType::Pointer rigidImage,
                                             ImageType::Pointer spatiallyNormalizedImage) const {
        if (!rigidImage || !spatiallyNormalizedImage) {
            throw std::invalid_argument("ADAD metric requires both rigid and normalized images");
        }

        ImageType::Pointer cerebellarGray = Common::nifti::loadImage(config_->getMaskPath("cerebral_gray"));
        if (!cerebellarGray) {
            throw std::runtime_error("Failed to load cerebellar gray mask");
        }

        ImageType::Pointer resampled = Common::image::resampleToMatch(cerebellarGray, spatiallyNormalizedImage);
        double meanGray = Common::image::calculateMeanInMask(resampled, cerebellarGray);
        if (meanGray <= 0.0) {
            throw std::runtime_error("Invalid cerebellar gray mean value computed for ADAD normalization");
        }

        ImageType::Pointer adniTemplate = Common::nifti::loadImage(config_->getTemplatePath("adni_pet_core"));
        if (!adniTemplate) {
            throw std::runtime_error("Failed to load ADNI PET core template");
        }

        ImageType::Pointer adniStyleImage = Common::image::resampleToMatch(adniTemplate, rigidImage);
        Common::image::divideVoxelsByValue(adniStyleImage, static_cast<float>(meanGray));
        return adniStyleImage;
    }

    DecoupledResult runDecoupler(const std::string& modality, ImageType::Pointer adniImage) const {
        if (!adniImage) {
            throw std::invalid_argument("ADAD metric requires an ADNI-style image");
        }

        const std::string key = modality == "tau" ? "tau_decoupler" : "abeta_decoupler";
        auto modelPaths = config_->getModelPaths(key);
        if (!modelPaths.empty()) {
            Decoupler decoupler(modelPaths);
            return decoupler.predict(adniImage);
        }
        Decoupler decoupler(config_->getModelPath(key));
        return decoupler.predict(adniImage);
    }

    void saveDecouplingOutputs(DecoupledResult& decoupled,
                               const MetricComputationOptions& options) const {
        auto it = options.stringParameters.find(kOutputPathParam);
        if (it == options.stringParameters.end() || it->second.empty()) {
            return;
        }

        const std::string& outputPath = it->second;
        try {
            Common::fs::ensureParentDirectory(outputPath);
            decoupled.SaveResults(outputPath);
        } catch (const std::exception& ex) {
            std::cerr << "[adad] Failed to save decoupling outputs: " << ex.what() << std::endl;
        }
    }

    std::map<std::string, float> buildTracerValues(const std::string& modality,
                                                   const DecoupledResult& decoupled) const {
        auto coeffs = loadTracerCoefficients(modality);
        std::map<std::string, float> values;
        if (coeffs.empty()) {
            values["ADAD_score"] = static_cast<float>(decoupled.ADADscore);
        } else {
            for (const auto& [tracer, pair] : coeffs) {
                float slope = pair.first;
                float intercept = pair.second;
                values[tracer] = slope * static_cast<float>(decoupled.ADADscore) + intercept;
            }
        }
        values["AD_probability"] = decoupled.ADprob * 100.0f;
        return values;
    }

    std::map<std::string, std::pair<float, float>> loadTracerCoefficients(const std::string& modality) const {
        std::map<std::string, std::pair<float, float>> coeffs;
        const std::string sectionName = "adad_" + modality + ".tracers";
        try {
            auto section = config_->getSection(sectionName);
            for (const auto& [key, value] : section) {
                auto dot = key.find('.');
                if (dot == std::string::npos) {
                    continue;
                }
                std::string tracer = key.substr(0, dot);
                std::string entry = key.substr(dot + 1);
                float parsedValue = 0.0f;
                try {
                    parsedValue = std::stof(value);
                } catch (...) {
                    continue;
                }

                auto& pair = coeffs[tracer];
                if (entry == "slope") {
                    pair.first = parsedValue;
                } else if (entry == "intercept") {
                    pair.second = parsedValue;
                }
            }
        } catch (...) {
            coeffs.clear();
        }
        return coeffs;
    }

    std::string resolveModality(const MetricComputationOptions& options) const {
        auto it = options.stringParameters.find(kModalityParam);
        if (it == options.stringParameters.end()) {
            return "abeta";
        }
        return normalizeModality(it->second);
    }

    ConfigurationPtr config_;
};

void printResults(const std::vector<MetricResult>& results, const std::string& modality) {
    if (results.empty()) {
        std::cout << "[adad] No metric results returned." << std::endl;
        return;
    }

    std::cout << "\n=== Refactor ADAD Results (" << modality << ") ===" << std::endl;
    for (const auto& metric : results) {
        std::cout << "Metric: " << metric.metricName << std::endl;
        std::cout << "ADAD score: " << metric.suvr << std::endl;
        for (const auto& [label, value] : metric.tracerValues) {
            std::cout << "  " << label << ": " << value << std::endl;
        }
    }
}

} // namespace

void registerMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (registry->hasModule("adad")) {
        return;
    }
    auto config = container.resolve<IConfiguration>();
    registry->registerModule(std::make_shared<ADADLogic>(config));
}

int runCommand(const ADADCLIOptions& options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[adad] Batch mode is not supported in the prototype refactor path yet." << std::endl;
        return EXIT_FAILURE;
    }

    ADADCLIOptions optionsCopy = options;
    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(optionsCopy);

    Common::path::requireOutputDirectoryExists(optionsCopy.outputPath);

    const std::string normalizedModality = normalizeModality(optionsCopy.modality);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = optionsCopy.configPath;
    bootstrapOptions.enableConfigDebug = optionsCopy.enableDebugOutput;
    bootstrapOptions.logTag = "adad";

    auto container = buildDefaultContainer(bootstrapOptions);

    ProcessingRequest request;
    request.outputPath = optionsCopy.outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = "adad";
    request.metricOptions.stringParameters[kModalityParam] = normalizedModality;
    request.metricOptions.stringParameters[kOutputPathParam] = optionsCopy.outputPath;

    request.normalization.inputPath = optionsCopy.inputPath;
    request.normalization.skip = optionsCopy.skipRegistration;
    request.normalization.options.useIterativeRigid = optionsCopy.useIterativeRigid;
    request.normalization.options.useManualFOV = optionsCopy.useManualFOV;
    request.normalization.options.enableDebugOutput = optionsCopy.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = optionsCopy.debugOutputBasePath;

    std::cout << "[adad] Starting processing: " << fullCommand << std::endl;
    auto app = resolveApplication(*container);

    try {
        auto response = app->run(request);
        std::cout << "\n[adad] Spatial normalization complete. Normalized image saved to "
                  << optionsCopy.outputPath << std::endl;
        printResults(response.metricResults, normalizedModality);
    } catch (const std::exception& ex) {
        std::cerr << "[adad] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[adad] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

} // namespace Pipeline::Metrics::ADAD


