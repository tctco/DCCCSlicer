#include <argparse/argparse.hpp>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <string>
#include <itkNiftiImageIOFactory.h>

// New architecture components
#include "config/Configuration.h"
#include "pipeline/ProcessingPipeline.h"
#include "pipeline/BatchProcessor.h"
#include "calculators/SUVrCalculator.h"
#include "utils/common.h"

const std::string SOFTWARE_VERSION = "3.0.0";

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
    // Note: No skipRegistration here since normalize is specifically for spatial normalization
};

/**
 * @brief Options for decoupling analysis
 */
struct DecoupleCommandOptions : BaseCommandOptions, SpatialNormalizationOptions {
    std::string modality; // "abeta" or "tau"
    bool skipRegistration = false;
};

/**
 * @brief Add common spatial normalization arguments to a parser
 */
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

/**
 * @brief Add common base arguments to a parser
 */
void addBaseArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("--input")
        .help("Input PET image path (or directory in batch mode)")
        .required();
    parser.add_argument("--output")
        .help("Output processed image path (or directory in batch mode)")
        .required();
    parser.add_argument("--config")
        .help("Configuration file path")
        .default_value("config.toml");
    parser.add_argument("--debug")
        .help("Enable debug mode")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--batch")
        .help("Enable batch processing mode")
        .default_value(false)
        .implicit_value(true);
}

/**
 * @brief Add SUVr-derived metric specific arguments to a parser
 */
void addSUVrDerivedMetricArguments(argparse::ArgumentParser& parser) {
    addBaseArguments(parser);
    addSpatialNormalizationArguments(parser);
    
    parser.add_argument("--suvr")
        .help("Include SUVr values in the output")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--skip-normalization")
        .help("Skip spatial normalization and calculate metrics directly")
        .default_value(false)
        .implicit_value(true);
}


/**
 * @brief Load configuration with logging
 */
std::shared_ptr<Configuration> loadConfigurationWithLogging(const std::string& configPath, bool debugMode = false) {
    auto config = std::make_shared<Configuration>();
    std::string actualConfigPath = configPath.empty() ? "config.toml" : Configuration::findConfigFile(configPath);
    
    bool loadSuccess = config->loadFromFile(actualConfigPath);
    std::cout << "Loading configuration from: " << actualConfigPath;
    
    loadSuccess ? std::cout << " [SUCCESS]" << std::endl : 
                  std::cout << " [FAILED] - using default configuration" << std::endl;
    
    debugMode && (config->printAllConfigurations(), true);
    
    return config;
}

/**
 * @brief Setup debug output path
 */
void setupDebugOutput(BaseCommandOptions& options) {
    if (options.enableDebugOutput && !options.outputPath.empty()) {
        std::filesystem::path outputFilePath(options.outputPath);
        std::string baseName = outputFilePath.stem().string();
        std::string directory = outputFilePath.parent_path().string();
        options.debugOutputBasePath = directory + "/" + baseName;
    }
}

/**
 * @brief Parse SUVr-derived metric options from command line
 */
SUVrDerivedMetricOptions parseSUVrDerivedMetricOptions(const argparse::ArgumentParser& program, const std::string& metricType) {
    SUVrDerivedMetricOptions options;
    options.inputPath = program.get<std::string>("--input");
    options.outputPath = program.get<std::string>("--output");
    options.configPath = program.get<std::string>("--config");
    options.includeSUVr = program.get<bool>("--suvr");
    options.skipRegistration = program.get<bool>("--skip-normalization");
    options.useIterativeRigid = program.get<bool>("--iterative");
    options.useManualFOV = program.get<bool>("--manual-fov");
    options.enableDebugOutput = program.get<bool>("--debug");
    options.batchMode = program.get<bool>("--batch");
    options.metricType = metricType;
    
    setupDebugOutput(options);
    return options;
}

/**
 * @brief Execute SUVr-derived metric command (generic)
 */
int executeSUVrDerivedMetricCommand(const argparse::ArgumentParser& program, const std::string& metricType, const std::string& fullCommand) {
    SUVrDerivedMetricOptions options = parseSUVrDerivedMetricOptions(program, metricType);
    
    // Initialize configuration
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    // Convert to ProcessingOptions
    ProcessingOptions procOptions;
    procOptions.skipRegistration = options.skipRegistration;
    procOptions.useIterativeRigid = options.useIterativeRigid;
    procOptions.useManualFOV = options.useManualFOV;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    
    // Set metric to calculate - each metric calculator already computes its own SUVr
    // The includeSUVr flag will be handled by the metric result printing logic
    procOptions.selectedMetric = metricType;
    
    if (options.batchMode) {
        // Batch processing
        auto processor = [config, procOptions](const std::string& inputPath, const std::string& outputPath) -> ProcessingResult {
            ProcessingPipeline pipeline(config);
            return pipeline.process(inputPath, outputPath, procOptions);
        };
        
        std::cout << "Starting " << metricType << " batch processing..." << std::endl;
        return BatchProcessor::runBatch(
            options.inputPath, 
            options.outputPath, 
            options.configPath, 
            SOFTWARE_VERSION,
            fullCommand,
            options.skipRegistration, 
            processor
        );
    }

    // Execute single file processing
    ProcessingPipeline pipeline(config);
    std::cout << "Starting " << metricType << " calculation: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
    
    // Output results
    std::cout << "\n=== " << metricType << " Results ===" << std::endl;
    
    // Print results for the specific metric only, with SUVr control
    for (const auto& metricResult : result.metricResults) {
        std::cout << "Metric: " << metricResult.metricName << std::endl;
        
        // Always show the main metric values (Centiloid, CenTauR, etc.)
        for (const auto& [tracer, value] : metricResult.tracerValues) {
            std::cout << tracer << ": " << value << std::endl;
        }
        std::cout << std::endl;
        // Only show SUVr if user requested it
        if (options.includeSUVr) {
          std::cout << "SUVr: " << metricResult.suvr << std::endl << std::endl;
        }
    }
    
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Execute centiloid command
 */
int executeCentiloidCommand(const argparse::ArgumentParser& centiloid_parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(centiloid_parser, "centiloid", fullCommand);
}

/**
 * @brief Execute CenTauR command
 */
int executeCenTauRCommand(const argparse::ArgumentParser& centaur_parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(centaur_parser, "centaur", fullCommand);
}

/**
 * @brief Execute CenTauRz command
 */
int executeCenTauRzCommand(const argparse::ArgumentParser& centaurz_parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(centaurz_parser, "centaurz", fullCommand);
}

/**
 * @brief Execute SUVr command
 */
int executeSUVrCommand(const argparse::ArgumentParser& suvr_parser, const std::string& fullCommand) {
    SUVrCommandOptions options;
    options.inputPath = suvr_parser.get<std::string>("--input");
    options.outputPath = suvr_parser.get<std::string>("--output");
    options.voiMaskPath = suvr_parser.get<std::string>("--voi-mask");
    options.refMaskPath = suvr_parser.get<std::string>("--ref-mask");
    options.configPath = suvr_parser.get<std::string>("--config");
    options.skipRegistration = suvr_parser.get<bool>("--skip-normalization");
    options.enableDebugOutput = suvr_parser.get<bool>("--debug");
    options.batchMode = suvr_parser.get<bool>("--batch");
    
    setupDebugOutput(options);
    
    // Initialize configuration
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    if (options.batchMode) {
        auto processor = [config, options](const std::string& inputPath, const std::string& outputPath) -> ProcessingResult {
            ImageType::Pointer inputImage;
             if (options.skipRegistration) {
                inputImage = Common::LoadNii(inputPath);
            } else {
                ProcessingOptions procOptions;
                procOptions.skipRegistration = false;
                procOptions.enableDebugOutput = options.enableDebugOutput;
                procOptions.debugOutputBasePath = options.debugOutputBasePath;
                procOptions.selectedMetric = ""; // Don't calculate standard metrics
                
                ProcessingPipeline pipeline(config);
                ProcessingResult result = pipeline.process(inputPath, outputPath, procOptions);
                inputImage = result.spatiallyNormalizedImage;
            }
            
            double suvr = SUVrCalculator::calculateSUVr(inputImage, options.voiMaskPath, options.refMaskPath);
            
            ProcessingResult result;
            MetricResult metric;
            metric.metricName = "CustomSUVr";
            metric.suvr = suvr;
            result.metricResults.push_back(metric);
            return result;
        };

        std::cout << "Starting SUVr batch processing..." << std::endl;
         return BatchProcessor::runBatch(
             options.inputPath, 
             options.outputPath, 
             options.configPath, 
             SOFTWARE_VERSION,
             fullCommand,
             options.skipRegistration, 
             processor
         );
    }

    // Load and process image
    ImageType::Pointer inputImage;
    if (options.skipRegistration) {
        inputImage = Common::LoadNii(options.inputPath);
    } else {
        // Perform spatial normalization first
        ProcessingOptions procOptions;
        procOptions.skipRegistration = false;
        procOptions.enableDebugOutput = options.enableDebugOutput;
        procOptions.debugOutputBasePath = options.debugOutputBasePath;
        procOptions.selectedMetric = ""; // Don't calculate any standard metrics
        
        ProcessingPipeline pipeline(config);
        ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
        inputImage = result.spatiallyNormalizedImage;
    }
    
    // Calculate SUVr with custom masks
    double suvr = SUVrCalculator::calculateSUVr(inputImage, options.voiMaskPath, options.refMaskPath);
    
    // Output results
    std::cout << "\n=== SUVr Results ===" << std::endl;
    std::cout << "VOI Mask: " << options.voiMaskPath << std::endl;
    std::cout << "Reference Mask: " << options.refMaskPath << std::endl;
    std::cout << "SUVr: " << suvr << std::endl;
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Execute normalize command
 */
int executeNormalizeCommand(const argparse::ArgumentParser& normalize_parser) {
    NormalizeCommandOptions options;
    options.inputPath = normalize_parser.get<std::string>("--input");
    options.outputPath = normalize_parser.get<std::string>("--output");
    options.configPath = normalize_parser.get<std::string>("--config");
    options.normalizationMethod = normalize_parser.get<std::string>("--method");
    options.useIterativeRigid = normalize_parser.get<bool>("--iterative");
    options.useManualFOV = normalize_parser.get<bool>("--manual-fov");
    options.enableADNIStyle = normalize_parser.get<bool>("--ADNI-PET-core");
    options.enableDebugOutput = normalize_parser.get<bool>("--debug");
    // Normalize doesn't support batch for metrics as per requirement (metric-only)
    
    setupDebugOutput(options);
    
    // Initialize configuration
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    // Perform spatial normalization only
    ProcessingOptions procOptions;
    procOptions.skipRegistration = false;
    procOptions.useIterativeRigid = options.useIterativeRigid;
    procOptions.useManualFOV = options.useManualFOV;
    procOptions.enableADNIStyle = options.enableADNIStyle;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = ""; // Don't calculate metrics
    
    ProcessingPipeline pipeline(config);
    std::cout << "Starting spatial normalization: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
    
    std::cout << "\n=== Normalization Complete ===" << std::endl;
    std::cout << "Output image: " << options.outputPath << std::endl;
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Execute decouple command
 */
int executeDecoupleCommand(const argparse::ArgumentParser& decouple_parser) {
    DecoupleCommandOptions options;
    options.inputPath = decouple_parser.get<std::string>("--input");
    options.outputPath = decouple_parser.get<std::string>("--output");
    options.modality = decouple_parser.get<std::string>("--modality");
    options.configPath = decouple_parser.get<std::string>("--config");
    options.skipRegistration = decouple_parser.get<bool>("--skip-normalization");
    options.enableDebugOutput = decouple_parser.get<bool>("--debug");
    
    setupDebugOutput(options);
    
    // Initialize configuration
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    // Convert to ProcessingOptions
    ProcessingOptions procOptions;
    procOptions.skipRegistration = options.skipRegistration;
    procOptions.decoupleModality = options.modality;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = ""; // Don't calculate standard metrics
    
    ProcessingPipeline pipeline(config);
    std::cout << "Starting decoupling analysis: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
    
    std::cout << "\n=== Decoupling Results ===" << std::endl;
    if (result.hasDecoupledResult) {
        result.decoupledResult.printResult();
    }
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[]) {
    itk::NiftiImageIOFactory::RegisterOneFactory();
    
    // Reconstruct full command line for logging
    std::string fullCommand;
    if (argc > 0) {
        fullCommand = argv[0];
        for (int i = 1; i < argc; ++i) {
            fullCommand += " " + std::string(argv[i]);
        }
    }

    try {
        // Setup main program with subcommands
        argparse::ArgumentParser program("CentiloidCalculator", SOFTWARE_VERSION);
        program.add_description("PET image analysis toolkit for quantitative biomarker calculation");
        
        // Create subcommand parsers in main function to ensure proper lifetime
        // Centiloid subcommand
        argparse::ArgumentParser centiloid_cmd("centiloid");
        centiloid_cmd.add_description("Calculate Centiloid metric for amyloid PET images");
        addSUVrDerivedMetricArguments(centiloid_cmd);

        // CenTauR subcommand
        argparse::ArgumentParser centaur_cmd("centaur");
        centaur_cmd.add_description("Calculate CenTauR metric for tau PET images");
        addSUVrDerivedMetricArguments(centaur_cmd);

        // CenTauRz subcommand
        argparse::ArgumentParser centaurz_cmd("centaurz");
        centaurz_cmd.add_description("Calculate CenTauRz metric for tau PET images (z-score)");
        addSUVrDerivedMetricArguments(centaurz_cmd);

        // SUVr subcommand  
        argparse::ArgumentParser suvr_cmd("suvr");
        suvr_cmd.add_description("Calculate SUVr metric with custom VOI and reference masks");
        addBaseArguments(suvr_cmd);
        addSpatialNormalizationArguments(suvr_cmd);
        suvr_cmd.add_argument("--voi-mask")
            .help("VOI (Volume of Interest) mask path")
            .required();
        suvr_cmd.add_argument("--ref-mask")
            .help("Reference region mask path")
            .required();
        suvr_cmd.add_argument("--skip-normalization")
            .help("Skip spatial normalization")
            .default_value(false)
            .implicit_value(true);
        
        // Normalize subcommand
        argparse::ArgumentParser normalize_cmd("normalize");
        normalize_cmd.add_description("Perform spatial normalization on PET images");
        addBaseArguments(normalize_cmd);
        addSpatialNormalizationArguments(normalize_cmd);
        normalize_cmd.add_argument("--method")
            .help("Normalization method")
            .default_value("rigid_voxelmorph");
        normalize_cmd.add_argument("--ADNI-PET-core")
            .help("Enable ADNI PET core style processing")
            .default_value(false)
            .implicit_value(true);
        
        // Decouple subcommand
        argparse::ArgumentParser decouple_cmd("decouple");
        decouple_cmd.add_description("Decouple PET images to extract AD-related components");
        addBaseArguments(decouple_cmd);
        addSpatialNormalizationArguments(decouple_cmd);
        decouple_cmd.add_argument("--modality")
            .help("Modality to decouple")
            .choices("abeta", "tau")
            .required();
        decouple_cmd.add_argument("--skip-normalization")
            .help("Skip spatial normalization")
            .default_value(false)
            .implicit_value(true);
        
        // Add subcommands to main program
        program.add_subparser(centiloid_cmd);
        program.add_subparser(centaur_cmd);
        program.add_subparser(centaurz_cmd);
        program.add_subparser(suvr_cmd);
        program.add_subparser(normalize_cmd);
        program.add_subparser(decouple_cmd);
        
        program.parse_args(argc, argv);
        
        // Execute appropriate subcommand
        if (program.is_subcommand_used("centiloid")) {
            return executeCentiloidCommand(centiloid_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaur")) {
            return executeCenTauRCommand(centaur_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaurz")) {
            return executeCenTauRzCommand(centaurz_cmd, fullCommand);
        } else if (program.is_subcommand_used("suvr")) {
            return executeSUVrCommand(suvr_cmd, fullCommand);
        } else if (program.is_subcommand_used("normalize")) {
            return executeNormalizeCommand(normalize_cmd);
        } else if (program.is_subcommand_used("decouple")) {
            return executeDecoupleCommand(decouple_cmd);
        } else {
            std::cerr << "No subcommand specified. Use --help for usage information." << std::endl;
            std::cerr << program;
            return EXIT_FAILURE;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
