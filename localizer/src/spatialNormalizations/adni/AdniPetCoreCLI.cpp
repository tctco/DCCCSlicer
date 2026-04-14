#include "AdniPetCoreCLI.h"
#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include <exception>
#include <iostream>

namespace Pipeline::SpatialNormalization::Adni {

namespace {

struct RunConfig {
    bool enableAdniPetCore = true;
    const char* logTag = "adni-pet-core";
};

int runNormalization(const NormalizeCommandOptions& options,
                     const RunConfig& config,
                     const std::string& fullCommand) {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << config.logTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = options.configPath;
    bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
    bootstrapOptions.logTag = config.logTag;

    auto container = Pipeline::buildCoreContainer(bootstrapOptions);
    SpatialNormalizationRequest request;
    request.inputPath = options.inputPath;
    request.skip = false;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.useManualFOV = options.useManualFOV;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = options.debugOutputBasePath;
    request.options.enableAdniPetCore = config.enableAdniPetCore;

    std::cout << "[" << config.logTag << "] Starting processing: " << fullCommand << std::endl;
    auto spatialService = container->resolve<ISpatialNormalizationService>();
    auto fileService = container->resolve<IFileService>();

    try {
        auto output = spatialService->normalize(request);
        fileService->saveNormalizedImage({output.spatiallyNormalizedImage, options.outputPath});
        std::cout << "\n[" << config.logTag << "] ADNI PET Core normalization complete. Output saved to "
                  << options.outputPath << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[" << config.logTag << "] Processing failed: " << ex.what() << std::endl;
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

} // namespace Pipeline::SpatialNormalization::Adni
