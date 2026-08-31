#include "AdniPetCoreCLI.h"
#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NiftiIO.h"
#include "../../core/common/NormalizationContracts.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/Version.h"
#include "../../core/di/Bootstrap.h"
#include "../../core/preprocessing/PetMotionCorrector.h"
#include "../../core/services/IFileService.h"
#include "../../core/services/ISpatialNormalizationService.h"
#include "../../metrics/shared/BatchLogging.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <vector>

namespace Pipeline::SpatialNormalization::Adni {

namespace {

struct RunConfig {
    bool enableAdniPetCore = true;
    const char* logTag = "adni-pet-core";
};

constexpr const char* kBatchOutputSuffix = "_ADNI_style.nii";
constexpr const char* kCoregSuffix = "_Coreg.nii";
constexpr const char* kAveragedSuffix = "_Coreg_Avg.nii";

using OutputPaths = std::array<std::string, 4>;

int deepestLevel(const NormalizeCommandOptions& options) {
    return *std::max_element(options.adniPetLevels.begin(), options.adniPetLevels.end());
}

bool exportsLevel(const NormalizeCommandOptions& options, int level) {
    return std::find(options.adniPetLevels.begin(), options.adniPetLevels.end(), level) !=
           options.adniPetLevels.end();
}

std::string addNiftiSuffix(const std::string& outputPath, const std::string& suffix) {
    const auto path = Common::path::fromUtf8(outputPath);
    const auto baseName = Common::fs::baseNameFromNifti(path);
    std::string extension = Common::path::toUtf8(path.extension());
    if (extension == ".gz" && path.stem().extension() == ".nii") {
        extension = ".nii.gz";
    }
    return Common::path::toUtf8(
        path.parent_path() / Common::path::fromUtf8(baseName + suffix + extension));
}

OutputPaths singleOutputPaths(const NormalizeCommandOptions& options) {
    OutputPaths paths;
    const int deepest = deepestLevel(options);
    for (const int level : options.adniPetLevels) {
        if (level == deepest) {
            paths[level] = options.outputPath;
        } else if (level == 1) {
            paths[level] = addNiftiSuffix(options.outputPath, "_Coreg");
        } else if (level == 2) {
            paths[level] = addNiftiSuffix(options.outputPath, "_Coreg_Avg");
        }
    }
    return paths;
}

OutputPaths batchOutputPaths(const NormalizeCommandOptions& options,
                             const std::filesystem::path& inputFile,
                             const std::filesystem::path& outputDir) {
    OutputPaths paths;
    if (exportsLevel(options, 1)) {
        paths[1] = Common::fs::buildOutputPath(inputFile, outputDir, kCoregSuffix);
    }
    if (exportsLevel(options, 2)) {
        paths[2] = Common::fs::buildOutputPath(inputFile, outputDir, kAveragedSuffix);
    }
    if (exportsLevel(options, 3)) {
        paths[3] = Common::fs::buildOutputPath(inputFile, outputDir, kBatchOutputSuffix);
    }
    return paths;
}

void ensureOutputDirectories(const OutputPaths& paths) {
    for (std::size_t level = 1; level < paths.size(); ++level) {
        if (!paths[level].empty() && !Common::fs::ensureParentDirectory(paths[level])) {
            throw std::runtime_error("Failed to prepare output directory: " + paths[level]);
        }
    }
}

class TemporaryNiftiFile {
public:
    TemporaryNiftiFile() = default;
    TemporaryNiftiFile(const TemporaryNiftiFile&) = delete;
    TemporaryNiftiFile& operator=(const TemporaryNiftiFile&) = delete;

    ~TemporaryNiftiFile() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }

    std::string create() {
        static std::atomic<unsigned long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("dccc_adni_pet_motion_" + std::to_string(timestamp) + "_" +
                 std::to_string(sequence.fetch_add(1)) + ".nii.gz");
        return Common::path::toUtf8(path_);
    }

private:
    std::filesystem::path path_;
};

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
                       const OutputPaths& outputPaths,
                       const std::string& debugOutputBasePath,
                       bool logCompletion) {
    try {
        ensureOutputDirectories(outputPaths);
        const int deepest = deepestLevel(options);
        TemporaryNiftiFile averagedDynamicInput;
        std::string normalizationInputPath = inputPath;
        const unsigned int inputDimension =
            Pipeline::Preprocessing::PetMotionCorrector::inspectImageDimension(inputPath);
        if (inputDimension == 4) {
            std::cout << "[" << config.logTag
                      << "] 4D PET detected; motion-correcting each frame to frame 0 before ADNI processing."
                      << std::endl;
            Pipeline::Preprocessing::PetMotionCorrector corrector;
            const bool retainCorrectedDynamic = exportsLevel(options, 1);
            const bool calculateAverage = deepest >= 2;
            auto motionResult = corrector.correct(
                inputPath, retainCorrectedDynamic, calculateAverage);

            if (retainCorrectedDynamic) {
                Pipeline::Preprocessing::PetMotionCorrector::saveCorrectedDynamicImage(
                    motionResult, outputPaths[1]);
                std::cout << "[" << config.logTag << "] Level 1 Coreg saved to "
                          << outputPaths[1] << std::endl;
            }
            if (calculateAverage) {
                normalizationInputPath = exportsLevel(options, 2)
                                             ? outputPaths[2]
                                             : averagedDynamicInput.create();
                Pipeline::Preprocessing::PetMotionCorrector::saveAveragedImage(
                    motionResult, normalizationInputPath);
                if (exportsLevel(options, 2)) {
                    std::cout << "[" << config.logTag << "] Level 2 Coreg, Avg saved to "
                              << outputPaths[2] << std::endl;
                }
            }
        } else if (inputDimension != 3) {
            throw std::invalid_argument(
                "adni-pet-core requires a 3D or 4D PET input; received a " +
                std::to_string(inputDimension) + "D image.");
        } else {
            if (exportsLevel(options, 1)) {
                throw std::invalid_argument(
                    "ADNI PET Core level 1 (Coreg) requires a 4D dynamic PET input.");
            }
            if (exportsLevel(options, 2)) {
                auto inputImage = Common::nifti::loadImage(inputPath);
                Common::nifti::saveImage(inputImage, outputPaths[2]);
                std::cout << "[" << config.logTag
                          << "] 3D PET treated as an already averaged input; Level 2 saved to "
                          << outputPaths[2] << std::endl;
            }
        }

        if (deepest < 3) {
            if (logCompletion) {
                std::cout << "\n[" << config.logTag << "] Requested export level(s) complete; "
                          << "stopped before Level 3 spatial standardization." << std::endl;
            }
            return EXIT_SUCCESS;
        }

        BootstrapOptions bootstrapOptions;
        bootstrapOptions.configPath = options.configPath;
        bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
        bootstrapOptions.logTag = config.logTag;
        auto container = Pipeline::buildCoreContainer(bootstrapOptions);
        auto spatialService = container->resolve<ISpatialNormalizationService>();
        auto fileService = container->resolve<IFileService>();

        SpatialNormalizationRequest request;
        request.inputPath = normalizationInputPath;
        request.skip = false;
        request.options.useIterativeRigid = options.useIterativeRigid;
        request.options.useManualFOV = options.useManualFOV;
        request.options.enableDebugOutput = options.enableDebugOutput;
        request.options.debugOutputBasePath = debugOutputBasePath;
        request.options.enableAdniPetCore = config.enableAdniPetCore;
        request.options.adniPetTracer = options.tracer;
        auto output = spatialService->normalize(request);
        fileService->saveNormalizedImage({output.spatiallyNormalizedImage, outputPaths[3]});
        if (logCompletion) {
            std::cout << "\n[" << config.logTag
                      << "] Level 3 Coreg, Avg, Std Img and Vox Siz saved to "
                      << outputPaths[3] << std::endl;
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
    std::cout << "[" << config.logTag << "] Starting processing: " << fullCommand << std::endl;
    const int result = processSingleImage(
        options,
        config,
        options.inputPath,
        singleOutputPaths(options),
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

    const bool hasBidsPattern = !options.bidsPattern.empty();
    std::vector<std::filesystem::path> files;
    try {
        files = Common::fs::collectInputNiftiFiles(inputDir, options.bidsPattern);
    } catch (const std::regex_error& ex) {
        std::cerr << "[" << config.logTag << "] Invalid --bids regex: "
                  << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    if (files.empty()) {
        std::cout << "[" << config.logTag << "] No "
                  << (hasBidsPattern ? "PET-BIDS NIfTI" : "NIfTI")
                  << " files found in " << Common::path::toUtf8(inputDir) << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[" << config.logTag << "] Starting batch processing of " << files.size()
              << " files: " << fullCommand << std::endl;

    auto batchInfo = Pipeline::Metrics::Shared::openBatchInfo(
        outputDir, fullCommand, SOFTWARE_VERSION, options.configPath, inputDir);
    Pipeline::Metrics::Shared::BatchSummary summary;

    for (const auto& inputFile : files) {
        summary.processed++;
        const auto outputPaths = batchOutputPaths(options, inputFile, outputDir);
        const std::string inputPath = Common::path::toUtf8(inputFile);
        const std::string fileLabel = Common::path::toUtf8(inputFile.filename());
        const std::string debugBase = resolveDebugBasePath(
            options, outputPaths[deepestLevel(options)]);

        try {
            const int result = processSingleImage(
                options,
                config,
                inputPath,
                outputPaths,
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
    if (options.batchMode || !options.bidsPattern.empty()) {
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
        parser.add_argument("--tracer")
            .help("PET tracer class (abeta, tau, or fdg); FDG uses iterative global mean normalization")
            .required()
            .choices("abeta", "tau", "fdg");
        parser.add_argument("--level")
            .help("Export one or more ADNI preprocessing levels: 1=Coreg, "
                  "2=Coreg Avg, 3=Coreg Avg Std Img and Vox Siz (default: 3)")
            .nargs(argparse::nargs_pattern::at_least_one)
            .append()
            .scan<'i', int>()
            .choices(1, 2, 3);
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
        options.bidsPattern = parser.get<std::string>("--bids");
        options.enableADNIStyle = true;
        options.tracer = parser.get<std::string>("--tracer");
        options.adniPetLevels =
            parser.present<std::vector<int>>("--level").value_or(std::vector<int>{3});
        std::sort(options.adniPetLevels.begin(), options.adniPetLevels.end());
        options.adniPetLevels.erase(
            std::unique(options.adniPetLevels.begin(), options.adniPetLevels.end()),
            options.adniPetLevels.end());

        if (!options.batchMode && options.bidsPattern.empty()) {
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
