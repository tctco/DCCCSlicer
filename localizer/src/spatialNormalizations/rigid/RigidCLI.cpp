#include "RigidCLI.h"
#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include <exception>
#include <iostream>

namespace Pipeline::SpatialNormalization::Rigid {

namespace {

class RigidCLI final : public ISpatialNormalizationCLI {
public:
    std::string getSubcommandName() const override {
        return "rigid";
    }

    std::string getDescription() const override {
        return "Run rigid-only alignment and update affine matrix without resampling";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        NormalizeCommandOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.enableDebugOutput = parser.get<bool>("--debug");

        if (!Common::fs::ensureParentDirectory(options.outputPath)) {
            std::cerr << "[rigid] Failed to prepare output directory: " << options.outputPath << std::endl;
            return EXIT_FAILURE;
        }

        if (options.enableDebugOutput) {
            setupDebugOutput(options);
        }

        std::cout << "[rigid] Starting processing: " << fullCommand << std::endl;

        BootstrapOptions bootstrapOptions;
        bootstrapOptions.configPath = options.configPath;
        bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
        bootstrapOptions.logTag = "rigid";

        auto container = Pipeline::buildCoreContainer(bootstrapOptions);
        auto spatialService = container->resolve<ISpatialNormalizationService>();
        auto fileService = container->resolve<IFileService>();

        SpatialNormalizationRequest request;
        request.inputPath = options.inputPath;
        request.skip = false;
        request.options.rigidOnly = true;
        request.options.enableDebugOutput = options.enableDebugOutput;
        request.options.debugOutputBasePath = options.debugOutputBasePath;

        try {
            auto output = spatialService->normalize(request);
            fileService->saveNormalizedImage({output.spatiallyNormalizedImage, options.outputPath});
            std::cout << "[rigid] Rigid alignment complete. Output saved to " << options.outputPath << std::endl;
            return EXIT_SUCCESS;
        } catch (const std::exception& ex) {
            std::cerr << "[rigid] Processing failed: " << ex.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
};

} // namespace

SpatialNormalizationCLIPtr createCLI() {
    return std::make_shared<RigidCLI>();
}

} // namespace Pipeline::SpatialNormalization::Rigid
