#include "BrainExtractionCLI.h"

#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NiftiIO.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/ConfigLoader.h"
#include "../../core/extraction/BrainExtractor.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace Pipeline::SpatialNormalization::Brain {

namespace {

std::string defaultMaskOutputPath(const std::string& outputPath) {
    const std::filesystem::path path = Common::path::fromUtf8(outputPath);
    const bool compressedNifti = path.extension() == ".gz" && path.stem().extension() == ".nii";
    const std::string baseName = compressedNifti
                                     ? Common::path::toUtf8(path.stem().stem())
                                     : Common::path::toUtf8(path.stem());
    const std::string extension = compressedNifti
                                      ? ".nii.gz"
                                      : Common::path::toUtf8(path.extension());
    return Common::path::toUtf8(
        path.parent_path() / Common::path::fromUtf8(baseName + "_mask" + extension));
}

class BrainExtractionCLI final : public ISpatialNormalizationCLI {
public:
    std::string getSubcommandName() const override {
        return "brain-extract";
    }

    std::string getDescription() const override {
        return "Extract the whole brain using inverse VoxelMorph registration";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
        parser.add_argument("--mask-output")
            .help("Output brain mask path (defaults to <output>_mask.nii[.gz])")
            .default_value(std::string{});
        parser.add_argument("--template-mask")
            .help("Override the configured padded template-space brain mask")
            .default_value(std::string{});
    }

    int execute(const argparse::ArgumentParser& parser,
                const std::string& fullCommand) override {
        BaseCommandOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        options.bidsPattern = parser.get<std::string>("--bids");

        if (options.batchMode || !options.bidsPattern.empty()) {
            std::cerr << "[brain-extract] --batch and --bids are not supported." << std::endl;
            return EXIT_FAILURE;
        }
        setupDebugOutput(options);

        std::string maskOutputPath = parser.get<std::string>("--mask-output");
        if (maskOutputPath.empty()) {
            maskOutputPath = defaultMaskOutputPath(options.outputPath);
        }
        if (!Common::fs::ensureParentDirectory(options.outputPath) ||
            !Common::fs::ensureParentDirectory(maskOutputPath)) {
            std::cerr << "[brain-extract] Failed to prepare output directory." << std::endl;
            return EXIT_FAILURE;
        }

        ConfigurationLoadOptions loadOptions;
        loadOptions.configPath = options.configPath;
        loadOptions.enableDebugOutput = options.enableDebugOutput;
        loadOptions.logTag = "brain-extract";
        auto config = loadConfigurationWithLogging(loadOptions);

        std::string templateMaskPath = parser.get<std::string>("--template-mask");
        if (templateMaskPath.empty()) {
            templateMaskPath = config->getMaskPath("padded_brain");
        }

        std::cout << "[brain-extract] Starting processing: " << fullCommand << std::endl;
        try {
            auto inputImage = Common::nifti::loadImage(options.inputPath);
            auto templateMask = Common::nifti::loadImage(templateMaskPath);
            BrainExtractor extractor(config);
            extractor.setDebugMode(options.enableDebugOutput, options.debugOutputBasePath);
            const auto result = extractor.extract(inputImage, templateMask);

            Common::nifti::saveImage(result.strippedImage, options.outputPath);
            Common::nifti::saveBinaryImage(result.brainMask, maskOutputPath);
            std::cout << "[brain-extract] Whole-brain extraction complete ("
                      << result.brainVoxelCount << " voxels)." << std::endl;
            std::cout << "[brain-extract] Extracted image saved to "
                      << options.outputPath << std::endl;
            std::cout << "[brain-extract] Brain mask saved to "
                      << maskOutputPath << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[brain-extract] Processing failed: " << ex.what() << std::endl;
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
};

}  // namespace

SpatialNormalizationCLIPtr createCLI() {
    return std::make_shared<BrainExtractionCLI>();
}

}  // namespace Pipeline::SpatialNormalization::Brain
