#include "CentaurzService.h"

#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../metrics/shared/BatchRunner.h"
#include "../../metrics/shared/DebugPathHelpers.h"
#include "../../metrics/shared/MetricRunResult.h"
#include "../../metrics/shared/SingleRunner.h"
#include "CenTauRzCalculator.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Pipeline::Metrics::Centaurz {

namespace {

constexpr const char* kLogTag = "centaurz";
constexpr const char* kBatchOutputSuffix = "_centaurz_refactor.nii";

struct TracerConfigSpec {
    const char* tracerName;
    const char* tracerKey;
    float defaultSlope;
    float defaultIntercept;
};

using TracerConfigSet = std::array<TracerConfigSpec, 6>;

constexpr TracerConfigSet kUniversalTracerSpecs = {{{"FTP", "ftp", 13.63f, -15.85f},
                                                    {"GTP1", "gtp1", 10.67f, -11.92f},
                                                    {"MK6240", "mk6240", 10.08f, -10.06f},
                                                    {"PI2620", "pi2620", 8.45f, -9.61f},
                                                    {"RO948", "ro948", 13.05f, -15.57f},
                                                    {"PM-PBB3", "pmpbb3", 16.73f, -15.34f}}};

constexpr TracerConfigSet kMesialTemporalTracerSpecs = {{{"FTP", "ftp", 10.42f, -12.11f},
                                                         {"GTP1", "gtp1", 7.88f, -8.75f},
                                                         {"MK6240", "mk6240", 7.28f, -7.01f},
                                                         {"PI2620", "pi2620", 6.03f, -6.83f},
                                                         {"RO948", "ro948", 11.76f, -13.08f},
                                                         {"PM-PBB3", "pmpbb3", 7.97f, -7.83f}}};

constexpr TracerConfigSet kMetaTemporalTracerSpecs = {{{"FTP", "ftp", 12.95f, -15.37f},
                                                       {"GTP1", "gtp1", 9.60f, -11.10f},
                                                       {"MK6240", "mk6240", 9.36f, -10.60f},
                                                       {"PI2620", "pi2620", 7.78f, -9.33f},
                                                       {"RO948", "ro948", 13.16f, -16.19f},
                                                       {"PM-PBB3", "pmpbb3", 11.78f, -11.21f}}};

constexpr TracerConfigSet kTemporoParietalTracerSpecs = {{{"FTP", "ftp", 13.75f, -15.92f},
                                                           {"GTP1", "gtp1", 10.84f, -12.27f},
                                                           {"MK6240", "mk6240", 9.98f, -10.15f},
                                                           {"PI2620", "pi2620", 8.21f, -9.52f},
                                                           {"RO948", "ro948", 13.05f, -15.62f},
                                                           {"PM-PBB3", "pmpbb3", 16.16f, -14.68f}}};

constexpr TracerConfigSet kFrontalTracerSpecs = {{{"FTP", "ftp", 11.61f, -13.01f},
                                                  {"GTP1", "gtp1", 9.41f, -9.71f},
                                                  {"MK6240", "mk6240", 10.05f, -8.91f},
                                                  {"PI2620", "pi2620", 9.07f, -9.01f},
                                                  {"RO948", "ro948", 12.61f, -13.45f},
                                                  {"PM-PBB3", "pmpbb3", 15.70f, -13.18f}}};

struct DetailedRegionSpec {
    const char* metricName;
    const char* maskKey;
    const char* configKey;
    const TracerConfigSet* tracerSpecs;
};

constexpr std::array<DetailedRegionSpec, 4> kDetailedRegionSpecs = {{
    {"CenTauRz.MesialTemporal",
     "centaurz_mesial_temporal_voi",
     "mesial_temporal",
     &kMesialTemporalTracerSpecs},
    {"CenTauRz.MetaTemporal",
     "centaurz_meta_temporal_voi",
     "meta_temporal",
     &kMetaTemporalTracerSpecs},
    {"CenTauRz.TemporoParietal",
     "centaurz_temporo_parietal_voi",
     "temporo_parietal",
     &kTemporoParietalTracerSpecs},
    {"CenTauRz.Frontal",
     "centaurz_frontal_voi",
     "frontal",
     &kFrontalTracerSpecs},
}};

std::string requireMaskPath(ConfigurationPtr config, const std::string& maskKey) {
    if (!config) {
        throw std::invalid_argument("CenTauRz requires configuration");
    }
    const std::string configKey = "masks." + maskKey;
    if (config->getString(configKey).empty()) {
        throw std::invalid_argument("Missing required configuration key: " + configKey);
    }
    return config->getMaskPath(maskKey);
}

std::map<std::string, CenTauRzCalculator::TracerParams> loadTracerParameters(
    ConfigurationPtr config,
    const std::string& baseKey,
    const TracerConfigSet& specs) {
    if (!config) {
        throw std::invalid_argument("CenTauRz requires configuration");
    }

    std::map<std::string, CenTauRzCalculator::TracerParams> params;
    for (const auto& spec : specs) {
        params[spec.tracerName] = {
            config->getFloat(baseKey + spec.tracerKey + ".slope", spec.defaultSlope),
            config->getFloat(baseKey + spec.tracerKey + ".intercept", spec.defaultIntercept)};
    }
    return params;
}

std::vector<CenTauRzCalculator::Input::DetailedRegion> loadDetailedRegions(ConfigurationPtr config) {
    std::vector<CenTauRzCalculator::Input::DetailedRegion> regions;
    regions.reserve(kDetailedRegionSpecs.size());
    for (const auto& spec : kDetailedRegionSpecs) {
        CenTauRzCalculator::Input::DetailedRegion region;
        region.metricName = spec.metricName;
        region.voiMaskPath = requireMaskPath(config, spec.maskKey);
        region.tracerParameters = loadTracerParameters(
            config,
            "centaurz.detailed_regions." + std::string(spec.configKey) + ".tracers.",
            *spec.tracerSpecs);
        regions.push_back(std::move(region));
    }
    return regions;
}

SpatialNormalizationRequest buildNormalizationRequest(const CentaurzCLIOptions& options,
                                                      const std::string& inputPath,
                                                      const std::string& debugBasePath) {
    SpatialNormalizationRequest request;
    request.inputPath = inputPath;
    request.skip = options.skipRegistration;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.useManualFOV = options.useManualFOV;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = options.enableDebugOutput ? debugBasePath : std::string{};
    return request;
}

} // namespace

CentaurzService::CentaurzService(ConfigurationPtr config,
                                 std::shared_ptr<ISpatialNormalizationService> spatialService,
                                 std::shared_ptr<IFileService> fileService)
    : config_(std::move(config)),
      spatialService_(std::move(spatialService)),
      fileService_(std::move(fileService)) {
    if (!config_ || !spatialService_ || !fileService_) {
        throw std::invalid_argument("CentaurzService requires configuration, normalization, and file services");
    }
}

int CentaurzService::run(CentaurzCLIOptions options, const std::string& fullCommand) {
    if (!options.batchMode) {
        Pipeline::Metrics::Shared::configureDerivedDebugBasePath(options);
    }
    if (options.batchMode) {
        return runBatch(options, fullCommand);
    }
    return runSingle(options, fullCommand);
}

Pipeline::Metrics::Shared::MetricRunResult CentaurzService::executeForImage(
    const CentaurzCLIOptions& options,
    const std::string& inputPath,
    const std::string& outputPath,
    const std::string& debugBasePath) const {
    auto normalizationOutput =
        spatialService_->normalize(buildNormalizationRequest(options, inputPath, debugBasePath));
    fileService_->saveNormalizedImage({normalizationOutput.spatiallyNormalizedImage, outputPath});

    CenTauRzCalculator::Input input;
    input.spatiallyNormalizedImage = normalizationOutput.spatiallyNormalizedImage;
    input.voiMaskPath = requireMaskPath(config_, "centaur_voi");
    input.refMaskPath = requireMaskPath(config_, "centaur_ref");
    input.tracerParameters = loadTracerParameters(config_, "centaurz.tracers.", kUniversalTracerSpecs);
    if (options.reportDetailedRegions) {
        input.detailedRegions = loadDetailedRegions(config_);
    }

    Pipeline::Metrics::Shared::MetricRunResult result;
    CenTauRzCalculator calculator;
    result.metricResults.push_back(calculator.calculate(input));
    if (options.reportDetailedRegions) {
        auto detailedResults = calculator.calculateDetailedRegions(input);
        result.metricResults.insert(
            result.metricResults.end(), detailedResults.begin(), detailedResults.end());
    }
    return result;
}

int CentaurzService::runSingle(const CentaurzCLIOptions& options, const std::string& fullCommand) const {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << kLogTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    Pipeline::Metrics::Shared::SingleRunnerHooks<CentaurzCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.resolveDebugBase = [](const CentaurzCLIOptions& runnerOptions, const std::string&) {
        return runnerOptions.enableDebugOutput ? runnerOptions.debugOutputBasePath : std::string{};
    };
    hooks.execute = [this](const CentaurzCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        return executeForImage(runnerOptions, inputPath, outputPath, debugBase);
    };
    hooks.logResults = [](const CentaurzCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        std::cout << "\n[" << kLogTag
                  << "] Spatial normalization complete. Normalized image saved to "
                  << runnerOptions.outputPath << std::endl;
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runSingle(options, fullCommand, hooks);
}

int CentaurzService::runBatch(const CentaurzCLIOptions& options, const std::string& fullCommand) const {
    Pipeline::Metrics::Shared::BatchRunnerHooks<CentaurzCLIOptions> hooks;
    hooks.logTag = kLogTag;
    hooks.batchOutputSuffix = kBatchOutputSuffix;
    hooks.resolveDebugBase = [](const CentaurzCLIOptions& runnerOptions, const std::string& outputPath) {
        return runnerOptions.enableDebugOutput
            ? Common::path::deriveDebugBasePath(outputPath)
            : std::string{};
    };
    hooks.execute = [this](const CentaurzCLIOptions& runnerOptions,
                           const std::string& inputPath,
                           const std::string& outputPath,
                           const std::string& debugBase) {
        return executeForImage(runnerOptions, inputPath, outputPath, debugBase);
    };
    hooks.logResults = [](const CentaurzCLIOptions& runnerOptions,
                          const Pipeline::Metrics::Shared::MetricRunResult& result) {
        for (const auto& metric : result.metricResults) {
            logMetricResult(metric, runnerOptions.includeSUVr);
        }
    };
    return Pipeline::Metrics::Shared::runBatch(options, fullCommand, hooks);
}

void CentaurzService::logMetricResult(const Metrics::MetricResult& result, bool includeSUVr) {
    std::cout << "\n=== Refactor CenTauRz Results ===" << std::endl;
    std::cout << "Metric: " << result.metricName << std::endl;
    for (const auto& [tracer, value] : result.tracerValues) {
        std::cout << "  " << tracer << ": " << value << std::endl;
    }
    if (includeSUVr || result.metricName != "CenTauRz") {
        std::cout << "  SUVr: " << result.suvr << std::endl;
    }
}

std::shared_ptr<CentaurzService> createService(ServiceContainer& container) {
    auto config = container.resolve<IConfiguration>();
    auto spatialService = container.resolve<ISpatialNormalizationService>();
    auto fileService = container.resolve<IFileService>();
    return std::make_shared<CentaurzService>(config, spatialService, fileService);
}

} // namespace Pipeline::Metrics::Centaurz
