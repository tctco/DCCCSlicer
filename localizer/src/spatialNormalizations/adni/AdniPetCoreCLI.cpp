#include "AdniPetCoreCLI.h"
#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/Version.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../../metrics/shared/BatchLogging.h"
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace Pipeline::SpatialNormalization::Adni {

namespace {

struct RunConfig {
    bool enableAdniPetCore = true;
    const char* logTag = "adni-pet-core";
};

constexpr const char* kBatchOutputSuffix = "_processed.nii";

std::string resolveDebugBasePath(const NormalizeCommandOptions& options, const std::string& outputPath) {
    if (!options.enableDebugOutput || outputPath.empty()) {
        return {};
    }

    std::filesystem::path outputFilePath = Common::path::fromUtf8(outputPath);
    std::string baseName = Common::path::toUtf8(outputFilePath.stem());
    std::string directory = Common::path::toUtf8(outputFilePath.parent_path());
    return directory.empty() ? baseName : directory + "/" + baseName;
}

int processSingleImage(const NormalizeCommandOptions& options,
                       const RunConfig& config,
                       const std::string& inputPath,
                       const std::string& outputPath,
                       const std::string& debugOutputBasePath,
                       bool logCompletion) {
    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = options.configPath;
    bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
    bootstrapOptions.logTag = config.logTag;

    auto container = Pipeline::buildCoreContainer(bootstrapOptions);
    SpatialNormalizationRequest request;
    request.inputPath = inputPath;
    request.skip = false;
    request.options.useIterativeRigid = options.useIterativeRigid;
    request.options.useManualFOV = options.useManualFOV;
    request.options.enableDebugOutput = options.enableDebugOutput;
    request.options.debugOutputBasePath = debugOutputBasePath;
    request.options.enableAdniPetCore = config.enableAdniPetCore;
    auto spatialService = container->resolve<ISpatialNormalizationService>();
    auto fileService = container->resolve<IFileService>();

    try {
        auto output = spatialService->normalize(request);
        fileService->saveNormalizedImage({output.spatiallyNormalizedImage, outputPath});
        if (logCompletion) {
            std::cout << "\n[" << config.logTag
                      << "] ADNI PET Core normalization complete. Output saved to "
                      << outputPath << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "[" << config.logTag << "] Processing failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int runSingleNormalization(const NormalizeCommandOptions& options,
                           const RunConfig& config,
                           const std::string& fullCommand) {
    if (!Common::fs::ensureParentDirectory(options.outputPath)) {
        std::cerr << "[" << config.logTag << "] Failed to prepare output directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[" << config.logTag << "] Starting processing: " << fullCommand << std::endl;
    const int result = processSingleImage(
        options,
        config,
        options.inputPath,
        options.outputPath,
        options.debugOutputBasePath,
        true);
    if (result != EXIT_SUCCESS) {
        return result;
    }

    std::cout << "[" << config.logTag << "] Processing completed successfully." << std::endl;
    return EXIT_SUCCESS;
}

int runBatchNormalization(const NormalizeCommandOptions& options,
                          const RunConfig& config,
                          const std::string& fullCommand) {
    const std::filesystem::path inputDir = Common::path::fromUtf8(options.inputPath);
    const std::filesystem::path outputDir = Common::path::fromUtf8(options.outputPath);

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "[" << config.logTag << "] Input directory does not exist: "
                  << options.inputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!Common::fs::ensureDirectory(outputDir)) {
        std::cerr << "[" << config.logTag << "] Output path must be a directory: "
                  << options.outputPath << std::endl;
        return EXIT_FAILURE;
    }
    if (!Common::fs::isDirectoryEmpty(outputDir)) {
        std::cerr << "[" << config.logTag
                  << "] Output directory must be empty for batch ADNI PET Core processing."
                  << std::endl;
        return EXIT_FAILURE;
    }

    const auto files = Common::fs::collectNiftiFiles(inputDir);
    if (files.empty()) {
        std::cout << "[" << config.logTag << "] No NIfTI files found in "
                  << Common::path::toUtf8(inputDir) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[" << config.logTag << "] Starting batch processing of " << files.size()
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
            const int result = processSingleImage(
                options,
                config,
                inputPath,
                outputPath,
                debugBase,
                false);
            if (result != EXIT_SUCCESS) {
                throw std::runtime_error("ADNI PET Core normalization failed.");
            }

            summary.succeeded++;
            std::cout << "[" << config.logTag << "][batch] Processed "
                      << fileLabel << std::endl;
            Pipeline::Metrics::Shared::appendSuccessEntry(
                batchInfo, fileLabel);
        } catch (const std::exception& ex) {
            summary.failed++;
            std::cerr << "[" << config.logTag << "][batch] Failed "
                      << fileLabel << ": " << ex.what() << std::endl;
            Pipeline::Metrics::Shared::appendFailureEntry(
                batchInfo, fileLabel, ex.what());
        }
    }

    Pipeline::Metrics::Shared::finalizeBatchInfo(batchInfo, summary);
    std::cout << "[" << config.logTag << "] Batch complete. Success: "
              << summary.succeeded << ", Failed: " << summary.failed << std::endl;

    return summary.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int runNormalization(const NormalizeCommandOptions& options,
                     const RunConfig& config,
                     const std::string& fullCommand) {
    if (options.batchMode) {
        return runBatchNormalization(options, config, fullCommand);
    }
    return runSingleNormalization(options, config, fullCommand);
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
        options.batchMode = parser.get<bool>("--batch");
        options.enableADNIStyle = true;

        if (!options.batchMode) {
            setupDebugOutput(options);
        }

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
