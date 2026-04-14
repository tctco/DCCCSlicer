#include "ADADService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "Decoupler.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::ADAD {

namespace {

constexpr const char* kLogTag = "adad";

SpatialNormalizationRequest buildNormalizationRequest(const ADADCLIOptions& options,
                                                      const std::string& inputPath,
                                                      const std::string& debugBasePath) {
    SpatialNormalizationRequest request;
    request.inputPath = inputPath;
    request.skip = options.skipRegistration;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.useManualFOV = options.useManualFOV;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = options.enableDebugOutput ? debugBasePath : std::string{};
    request.options.enableAdniPetCore = true;
    return request;
}

} // namespace

ADADService::ADADService(ConfigurationPtr config,
                         std::shared_ptr<ISpatialNormalizationService> spatialService,
                         std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("ADADService requires configuration, normalization, and file services");
    }
}

int ADADService::run(ADADCLIOptions options, const std::string& fullCommand) {
    if (options.batchMode) {
        std::cerr << "[adad] Batch mode is not supported." << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    return runSingle(options, fullCommand);
}

Metrics::MetricResult ADADService::executeForImage(const ADADCLIOptions& options,
                                                   const std::string& inputPath,
                                                   const std::string& outputPath,
                                                   const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    const std::string modality = normalizeModality(options.modality);
    const std::string key = modality == "tau" ? "tau_decoupler" : "abeta_decoupler";
    DecoupledResult decoupled;
    auto modelPaths = config_->getModelPaths(key);
    if (!modelPaths.empty()) {
        Decoupler decoupler(modelPaths);
        decoupled = decoupler.predict(normalizationOutput.spatiallyNormalizedImage);
    } else {
        Decoupler decoupler(config_->getModelPath(key));
        decoupled = decoupler.predict(normalizationOutput.spatiallyNormalizedImage);
    }
    Common::fs::ensureParentDirectory(outputPath);
    decoupled.SaveResults(outputPath);

    Metrics::MetricResult metric;
    metric.metricName = modality == "tau" ? "ADAD_tau" : "ADAD_abeta";
    metric.suvr = decoupled.ADADscore;
    metric.tracerValues = buildTracerValues(modality, decoupled);
    return metric;
}

int ADADService::runSingle(const ADADCLIOptions& options, const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<ADADCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const ADADCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const ADADCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        Pipeline::Metrics::Shared::MetricRunResult result;
        result.metricResults.push_back(
            executeForImage(runnerOptions, inputPath, outputPath, debugBase));
        return result;
    };
    hooks.logResults = [](const ADADCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        std::cout << "\n[" << kLogTag
                  << "] Spatial normalization complete. Normalized image saved to "
                  << runnerOptions.outputPath << std::endl;
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, normalizeModality(runnerOptions.modality));
        }
    };
    return Pipeline::Metrics::Shared::runSingle(options, fullCommand, hooks);
}

std::map<std::string, float> ADADService::buildTracerValues(const std::string& modality,
                                                            const DecoupledResult& decoupled) const {
    auto coeffs = loadTracerCoefficients(modality);
    std::map<std::string, float> values;
    if (coeffs.empty()) {
        values["ADAD_score"] = static_cast<float>(decoupled.ADADscore);
    } else {
        for (const auto& [tracer, pair] : coeffs) {
            values[tracer] = pair.first * static_cast<float>(decoupled.ADADscore) + pair.second;
        }
    }
    values["AD_probability"] = decoupled.ADprob * 100.0f;
    return values;
}

std::map<std::string, std::pair<float, float>> ADADService::loadTracerCoefficients(
    const std::string& modality) const {
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

std::string ADADService::normalizeModality(const std::string& raw) {
    std::string modality = Common::path::toLower(raw);
    if (modality != "tau") {
        modality = "abeta";
    }
    return modality;
}

void ADADService::logMetricResult(const Metrics::MetricResult& result, const std::string& modality) {
    std::cout << "\n=== Refactor ADAD Results (" << modality << ") ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    std::cout << "ADAD score: " << result.suvr << std::endl;
    for (const auto& [label, value] : result.tracerValues) {
        std::cout << "  " << label << ": " << value << std::endl;
    }
}

std::shared_ptr<ADADService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<ADADService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::ADAD
