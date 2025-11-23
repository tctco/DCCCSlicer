#pragma once
#include <string>
#include <argparse/argparse.hpp>
#include "../config/Version.h"

/**
 * @brief Command-specific option structures
 */
struct BaseCommandOptions {
    std::string inputPath;
    std::string outputPath;
    std::string configPath;
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool batchMode = false;
};

/**
 * @brief Spatial normalization related options
 */
struct SpatialNormalizationOptions {
    bool useIterativeRigid = false;
    bool useManualFOV = false;
};

/**
 * @brief Options for SUVr-derived metrics (Centiloid, CenTauR, CenTauRz)
 */
struct SUVrDerivedMetricOptions : BaseCommandOptions, SpatialNormalizationOptions {
    bool includeSUVr = false;
    bool skipRegistration = false;
    std::string metricType; // "centiloid", "centaur", "centaurz"
};

/**
 * @brief Options for custom SUVr calculation
 */
struct SUVrCommandOptions : BaseCommandOptions, SpatialNormalizationOptions {
    std::string voiMaskPath;
    std::string refMaskPath;
    bool skipRegistration = false;
};

/**
 * @brief Options for spatial normalization only
 */
struct NormalizeCommandOptions : BaseCommandOptions, SpatialNormalizationOptions {
    bool enableADNIStyle = false;
    std::string normalizationMethod = "rigid_voxelmorph";
};

/**
 * @brief Options for decoupling analysis
 */
struct DecoupleCommandOptions : BaseCommandOptions, SpatialNormalizationOptions {
    std::string modality; // "abeta" or "tau"
    bool skipRegistration = false;
};

// Shared Argument Parsers
void addBaseArguments(argparse::ArgumentParser& parser);
void addSpatialNormalizationArguments(argparse::ArgumentParser& parser);
void addSUVrDerivedMetricArguments(argparse::ArgumentParser& parser);

// Helper functions
void setupDebugOutput(BaseCommandOptions& options);
