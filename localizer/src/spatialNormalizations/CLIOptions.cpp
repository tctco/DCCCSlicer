#include "CLIOptions.h"

#include <filesystem>

void addBaseArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("--input")
        .help("Input PET image path (or directory in batch mode)")
        .required();
    parser.add_argument("--output")
        .help("Output processed image path (or directory in batch mode)")
        .required();
    parser.add_argument("--config")
        .help("Configuration file path")
        .default_value(std::string{"config.toml"});
    parser.add_argument("--debug")
        .help("Enable debug mode")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--batch")
        .help("Enable batch processing mode")
        .default_value(false)
        .implicit_value(true);
}

void addSpatialNormalizationArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("-i", "--iterative")
        .help("Use iterative rigid transformation")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("-m", "--manual-fov")
        .help("Use manual FOV placement")
        .default_value(false)
        .implicit_value(true);
}

void setupDebugOutput(BaseCommandOptions& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        return;
    }

    std::filesystem::path outputFilePath(options.outputPath);
    std::string baseName = outputFilePath.stem().string();
    std::string directory = outputFilePath.parent_path().string();

    if (!directory.empty()) {
        options.debugOutputBasePath = directory + "/" + baseName;
    } else {
        options.debugOutputBasePath = baseName;
    }
}


