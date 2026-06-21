#include "RigidCLI.h"
#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/Version.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../../metrics/shared/BatchLogging.h"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <vector>

namespace Pipeline::SpatialNormalization::Rigid {

namespace {

constexpr const char* kBatchOutputSuffix = "_rigid_aligned.nii";

std::string resolveDebugBasePath(const NormalizeCommandOptions& options, const std::string& outputPath) {
    if (!options.enableDebugOutput || outputPath.empty()) {
        return {};
    }
    return Common::path::deriveDebugBasePath(outputPath);
}

int processSingleImage(const NormalizeCommandOptions& options,
                       const std::string& inputPath,
                       const std::string& outputPath,
                       const std::string& debugOutputBasePath,
                       bool logCompletion) {
    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = options.configPath;
    bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
    bootstrapOptions.logTag = "rigid";

    auto container = Pipeline::buildCoreContainer(bootstrapOptions);
    auto spatialService = container->resolve<ISpatialNormalizationService>();
    auto fileService = container->resolve<IFileService>();

    SpatialNormalizationRequest request;
    request.inputPath = inputPath;
    request.skip = false;
    request.options.rigidOnly = true;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = debugOutputBasePath;

    try {
        auto output = spatialService->normalize(request);
        fileService->saveNormalizedImage({output.spatiallyNormalizedImage, outputPath});
        if (logCompletion) {
            std::cout << "[rigid] Rigid alignment complete. Output saved to "
                      << outputPath << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "[rigid] Processing failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int runSingleRigid(const NormalizeCommandOptions& options, const std::string& fullCommand) {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[rigid] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[rigid] Starting processing: " << fullCommand << std::endl;
    return processSingleImage(
        options,
        options.inputPath,
        options.outputPath,
        options.debugOutputBasePath,
        true);
}

int runBatchRigid(const NormalizeCommandOptions& options, const std::string& fullCommand) {
    const std::filesystem::path inputDir = Common::path::fromUtf8(options.inputPath);
    const std::filesystem::path outputDir = Common::path::fromUtf8(options.outputPath);
    const bool hasBidsPattern = !options.bidsPattern.empty();

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "[rigid] Input directory does not exist: "
                  << options.inputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!Common::fs::ensureDirectory(outputDir)) {
        std::cerr << "[rigid] Output path must be a directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!Common::fs::isDirectoryEmpty(outputDir)) {
        std::cerr << "[rigid] Output directory must be empty for batch rigid processing."
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<std::filesystem::path> files;
    try {
        files = Common::fs::collectInputNiftiFiles(inputDir, options.bidsPattern);
    } catch (const std::regex_error& ex) {
        std::cerr << "[rigid] Invalid --bids regex: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    if (files.empty()) {
        std::cout << "[rigid] No " << (hasBidsPattern ? "PET-BIDS NIfTI" : "NIfTI")
                  << " files found in " << Common::path::toUtf8(inputDir) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[rigid] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    auto batchInfo = Pipeline::Metrics::Shared::openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    Pipeline::Metrics::Shared::BatchSummary summary;

    for (const auto& inputFile : files) {
        summary.processed++;
        const std::string outputPath =
            Common::fs::buildOutputPath(inputFile, outputDir, kBatchOutputSuffix);
        const std::string inputPath = Common::path::toUtf8(inputFile);
        const std::string fileLabel = Common::path::toUtf8(inputFile.filename());
        const std::string debugBase = resolveDebugBasePath(options, outputPath);

        try {
            const int result = processSingleImage(options, inputPath, outputPath, debugBase, false);
            if (result != EXIT_SUCCESS) {
                throw std::runtime_error("Rigid alignment failed.");
            }

            summary.succeeded++;
            std::cout << "[rigid][batch] Processed " << fileLabel << std::endl;
            Pipeline::Metrics::Shared::appendSuccessEntry(batchInfo, fileLabel);
        } catch (const std::exception& ex) {
            summary.failed++;
            std::cerr << "[rigid][batch] Failed " << fileLabel << ": "
                      << ex.what() << std::endl;
            Pipeline::Metrics::Shared::appendFailureEntry(batchInfo, fileLabel, ex.what());
        }
    }

    Pipeline::Metrics::Shared::finalizeBatchInfo(batchInfo, summary);
    std::cout << "[rigid] Batch complete. Success: " << summary.succeeded
              << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

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
        parser.add_argument("-i", "--iterative")
            .help("Use iterative rigid transformation")
            .default_value(false)
            .implicit_value(true);
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        NormalizeCommandOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        options.bidsPattern = parser.get<std::string>("--bids");

        if (!options.batchMode && options.bidsPattern.empty() && options.enableDebugOutput) {
            setupDebugOutput(options);
        }

        if (options.batchMode || !options.bidsPattern.empty()) {
            return runBatchRigid(options, fullCommand);
        }
        return runSingleRigid(options, fullCommand);
    }
};

} // namespace

SpatialNormalizationCLIPtr createCLI() {
    return std::make_shared<RigidCLI>();
}

} // namespace Pipeline::SpatialNormalization::Rigid
