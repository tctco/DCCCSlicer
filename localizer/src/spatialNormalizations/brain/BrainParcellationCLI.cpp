#include "BrainParcellationCLI.h"

#include "../CLIOptions.h"
#include "../../core/common/Filesystem.h"
#include "../../core/common/NiftiIO.h"
#include "../../core/common/PathUtils.h"
#include "../../core/config/ConfigLoader.h"
#include "../../core/extraction/BrainParcellator.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Pipeline::SpatialNormalization::BrainParcellation {

namespace {

const BrainParcellator::Region& findRegion(
    const BrainParcellator::Result& result, const std::string& key) {
    for (const auto& region : result.regions) {
        if (region.key == key) {
            return region;
        }
    }
    throw std::runtime_error("Missing parcellation result: " + key);
}

void writeSummary(const BrainParcellator::Result& result,
                  const std::filesystem::path& outputPath) {
    const auto& ventricles = findRegion(result, "ventricles");
    const auto& leftHippocampus = findRegion(result, "left_hippocampus");
    const auto& rightHippocampus = findRegion(result, "right_hippocampus");
    const auto& wholeBrain = findRegion(result, "whole_brain");
    const auto& leftEntorhinal = findRegion(result, "left_entorhinal");
    const auto& rightEntorhinal = findRegion(result, "right_entorhinal");
    const auto& leftFusiform = findRegion(result, "left_fusiform");
    const auto& rightFusiform = findRegion(result, "right_fusiform");
    const auto& leftMiddleTemporal = findRegion(result, "left_middle_temporal");
    const auto& rightMiddleTemporal = findRegion(result, "right_middle_temporal");
    const auto& intracranial = findRegion(result, "intracranial");

    std::ofstream output(outputPath);
    if (!output) {
        throw std::runtime_error(
            "Unable to create volume summary: " +
            Common::path::toUtf8(outputPath));
    }
    output << std::fixed << std::setprecision(6);
    output << "{\n"
           << "  \"units\": \"mL\",\n"
           << "  \"method\": \"inverse-warped FreeSurfer fsaverage "
              "Desikan-Killiany + aseg\",\n"
           << "  \"Ventricles\": {\"volume_ml\": "
           << ventricles.volumeMl
           << ", \"mask\": \"ventricles_mask.nii.gz\"},\n"
           << "  \"Hippocampus\": {\"left_volume_ml\": "
           << leftHippocampus.volumeMl
           << ", \"right_volume_ml\": " << rightHippocampus.volumeMl
           << ", \"bilateral_volume_ml\": "
           << leftHippocampus.volumeMl + rightHippocampus.volumeMl
           << ", \"left_mask\": \"left_hippocampus_mask.nii.gz\", "
              "\"right_mask\": \"right_hippocampus_mask.nii.gz\"},\n"
           << "  \"WholeBrain\": {\"volume_ml\": "
           << wholeBrain.volumeMl
           << ", \"mask\": \"whole_brain_mask.nii.gz\"},\n"
           << "  \"Entorhinal\": {\"left_volume_ml\": "
           << leftEntorhinal.volumeMl
           << ", \"right_volume_ml\": " << rightEntorhinal.volumeMl
           << ", \"bilateral_volume_ml\": "
           << leftEntorhinal.volumeMl + rightEntorhinal.volumeMl
           << ", \"left_mask\": \"left_entorhinal_mask.nii.gz\", "
              "\"right_mask\": \"right_entorhinal_mask.nii.gz\"},\n"
           << "  \"Fusiform\": {\"left_volume_ml\": "
           << leftFusiform.volumeMl
           << ", \"right_volume_ml\": " << rightFusiform.volumeMl
           << ", \"bilateral_volume_ml\": "
           << leftFusiform.volumeMl + rightFusiform.volumeMl
           << ", \"left_mask\": \"left_fusiform_mask.nii.gz\", "
              "\"right_mask\": \"right_fusiform_mask.nii.gz\"},\n"
           << "  \"MidTemp\": {\"left_volume_ml\": "
           << leftMiddleTemporal.volumeMl
           << ", \"right_volume_ml\": " << rightMiddleTemporal.volumeMl
           << ", \"bilateral_volume_ml\": "
           << leftMiddleTemporal.volumeMl + rightMiddleTemporal.volumeMl
           << ", \"left_mask\": \"left_middle_temporal_mask.nii.gz\", "
              "\"right_mask\": \"right_middle_temporal_mask.nii.gz\"},\n"
           << "  \"ICV\": {\"volume_ml\": " << intracranial.volumeMl
           << ", \"mask\": \"intracranial_mask.nii.gz\", "
              "\"definition\": \"atlas-warped intracranial mask proxy; "
              "not FreeSurfer eTIV\"}\n"
           << "}\n";
}

class BrainParcellationCLI final : public ISpatialNormalizationCLI {
public:
    std::string getSubcommandName() const override {
        return "brain-parcellate";
    }

    std::string getDescription() const override {
        return "Create ADNI-style regional masks and volume estimates";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
        parser.add_argument("--atlas")
            .help("Override the configured padded aparc+aseg atlas")
            .default_value(std::string{});
        parser.add_argument("--intracranial-template")
            .help("Override the configured template-space intracranial mask")
            .default_value(std::string{});
        parser.add_argument("--threshold")
            .help("Threshold applied to inverse-warped binary masks")
            .default_value(0.5f);
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
            std::cerr << "[brain-parcellate] --batch and --bids are not supported."
                      << std::endl;
            return EXIT_FAILURE;
        }
        setupDebugOutput(options);

        const std::filesystem::path outputDir =
            Common::path::fromUtf8(options.outputPath);
        if (!Common::fs::ensureDirectory(outputDir)) {
            std::cerr << "[brain-parcellate] Failed to prepare output directory."
                      << std::endl;
            return EXIT_FAILURE;
        }

        ConfigurationLoadOptions loadOptions;
        loadOptions.configPath = options.configPath;
        loadOptions.enableDebugOutput = options.enableDebugOutput;
        loadOptions.logTag = "brain-parcellate";
        auto config = loadConfigurationWithLogging(loadOptions);

        std::string atlasPath = parser.get<std::string>("--atlas");
        if (atlasPath.empty()) {
            atlasPath = config->getMaskPath("padded_aparc_aseg");
        }
        std::string intracranialPath =
            parser.get<std::string>("--intracranial-template");
        if (intracranialPath.empty()) {
            intracranialPath = config->getMaskPath("padded_brain");
        }

        std::cout << "[brain-parcellate] Starting processing: "
                  << fullCommand << std::endl;
        try {
            auto inputImage = Common::nifti::loadImage(options.inputPath);
            auto atlas = Common::nifti::loadImage(atlasPath);
            auto intracranialTemplate =
                Common::nifti::loadImage(intracranialPath);
            BrainParcellator parcellator(config);
            parcellator.setDebugMode(
                options.enableDebugOutput, options.debugOutputBasePath);
            const auto result = parcellator.parcellate(
                inputImage, atlas, intracranialTemplate,
                parser.get<float>("--threshold"));

            for (const auto& region : result.regions) {
                const auto path = outputDir /
                    Common::path::fromUtf8(region.key + "_mask.nii.gz");
                Common::nifti::saveBinaryImage(
                    region.mask, Common::path::toUtf8(path));
            }
            const auto summaryPath = outputDir / "volumes.json";
            writeSummary(result, summaryPath);

            std::cout << "[brain-parcellate] Regional masks and volumes saved to "
                      << Common::path::toUtf8(outputDir) << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[brain-parcellate] Processing failed: "
                      << ex.what() << std::endl;
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
};

}  // namespace

SpatialNormalizationCLIPtr createCLI() {
    return std::make_shared<BrainParcellationCLI>();
}

}  // namespace Pipeline::SpatialNormalization::BrainParcellation
