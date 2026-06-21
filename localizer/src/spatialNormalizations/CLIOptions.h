#pragma once

#include <argparse/argparse.hpp>
#include <string>

struct BaseCommandOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath = "config.toml";
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool batchMode = false;
    std::string bidsPattern;
};

struct SpatialNormalizationOptions {
    bool useIterativeRigid = false;
    bool useManualFOV = false;
};

struct NormalizeCommandOptions : BaseCommandOptions, SpatialNormalizationOptions {
    bool enableADNIStyle = false;
    std::string normalizationMethod = "rigid_voxelmorph";
};

void addBaseArguments(argparse::ArgumentParser& parser);
void addSpatialNormalizationArguments(argparse::ArgumentParser& parser);
void setupDebugOutput(BaseCommandOptions& options);

