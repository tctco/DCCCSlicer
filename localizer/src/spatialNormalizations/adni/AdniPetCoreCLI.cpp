#include "AdniPetCoreCLI.h"
#include "../../cli/Options.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/ProcessingContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../NullMetric.h"
#include <iostream>

namespace RefactorPipeline::SpatialNormalization::Adni {

namespace {

struct RunConfig {
    bool enableAdniPetCore = true;
    const char* logTag = "adni-pet-core";
};

int runNormalization(const NormalizeCommandOptions& options,
                     const RunConfig& config,
                     const std::string& fullCommand) {
    refactorCommon::fs::ensureParentDirectory(options.outputPath);

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = options.configPath;
    bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
    bootstrapOptions.logTag = config.logTag;

    auto container = RefactorPipeline::buildDefaultContainer(bootstrapOptions);
    SpatialNormalization::registerNullMetric(*container);

    ProcessingRequest request;
    request.outputPath = options.outputPath;
    request.persistNormalizedImage = true;
    request.computeMetrics = true;
    request.metricOptions.metricName = SpatialNormalization::kNullMetricName;

    request.normalization.inputPath = options.inputPath;
    request.normalization.skip = false;
    request.normalization.options.useIterativeRigid = options.useIterativeRigid;
    request.normalization.options.useManualFOV = options.useManualFOV;
    request.normalization.options.enableDebugOutput = options.enableDebugOutput;
    request.normalization.options.debugOutputBasePath = options.debugOutputBasePath;
    request.normalization.options.enableAdniPetCore = config.enableAdniPetCore;

    std::cout << "[" << config.logTag << "] Starting processing: " << fullCommand << std::endl;
    auto app = RefactorPipeline::resolveApplication(*container);

    try {
        auto response = app->run(request);
        (void)response;
        std::cout << "\n[" << config.logTag << "] ADNI PET Core normalization complete. Output saved to "
                  << options.outputPath << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[" << config.logTag << "] Pipeline failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[" << config.logTag << "] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

class AdniPetCoreCLI final : public ISpatialNormalizationCLI {
public:
    std::string getSubcommandName() const override {
        return "adni-pet-core";
    }

    std::string getDescription() const override {
        return "Run ADNI PET Core compliant spatial normalization";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
        addSpatialNormalizationArguments(parser);

        parser.add_argument("--method")
            .help("Normalization method")
            .default_value("rigid_voxelmorph");
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        NormalizeCommandOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.normalizationMethod = parser.get<std::string>("--method");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.useManualFOV = parser.get<bool>("--manual-fov");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.enableADNIStyle = true;

        setupDebugOutput(options);

        RunConfig runConfig;
        runConfig.enableAdniPetCore = true;
        runConfig.logTag = "adni-pet-core";
        return runNormalization(options, runConfig, fullCommand);
    }
};

} // namespace

SpatialNormalizationCLIPtr createCLI() {
    return std::make_shared<AdniPetCoreCLI>();
}

} // namespace RefactorPipeline::SpatialNormalization::Adni

