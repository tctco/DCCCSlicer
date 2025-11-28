#include "Commands.h"
#include "Options.h"
#include "../config/Configuration.h"
#include "../config/Version.h"
#include "../pipeline/ProcessingPipeline.h"
#include "../pipeline/BatchProcessor.h"
#include "../calculators/SUVrCalculator.h"
#include "../utils/common.h"
#include <iostream>

// Helper to load config
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

FillStatesCommandOptions parseFillStatesOptions(const argparse::ArgumentParser& program) {
    FillStatesCommandOptions options;
    options.inputPath = program.get<std::string>("--input");
    options.outputPath = program.get<std::string>("--output");
    options.configPath = program.get<std::string>("--config");
    options.includeSUVr = program.get<bool>("--suvr");
    options.skipRegistration = program.get<bool>("--skip-normalization");
    options.useIterativeRigid = program.get<bool>("--iterative");
    options.useManualFOV = program.get<bool>("--manual-fov");
    options.enableDebugOutput = program.get<bool>("--debug");
    options.batchMode = program.get<bool>("--batch");
    options.metricType = "fillstates";
    options.tracer = program.get<std::string>("--tracer");

    setupDebugOutput(options);
    return options;
}

int executeSUVrDerivedMetricCommand(const argparse::ArgumentParser& program, const std::string& metricType, const std::string& fullCommand) {
    SUVrDerivedMetricOptions options = parseSUVrDerivedMetricOptions(program, metricType);
    
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    ProcessingOptions procOptions;
    procOptions.skipRegistration = options.skipRegistration;
    procOptions.useIterativeRigid = options.useIterativeRigid;
    procOptions.useManualFOV = options.useManualFOV;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = metricType;
    
    if (options.batchMode) {
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

    ProcessingPipeline pipeline(config);
    std::cout << "Starting " << metricType << " calculation: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
    
    std::cout << "\n=== " << metricType << " Results ===" << std::endl;
    
    for (const auto& metricResult : result.metricResults) {
        std::cout << "Metric: " << metricResult.metricName << std::endl;
        for (const auto& [tracer, value] : metricResult.tracerValues) {
            std::cout << tracer << ": " << value << std::endl;
        }
        std::cout << std::endl;
        if (options.includeSUVr) {
          std::cout << "SUVr: " << metricResult.suvr << std::endl << std::endl;
        }
    }
    
    std::cout << "Processing completed successfully!" << std::endl;
    return EXIT_SUCCESS;
}

int executeCentiloidCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(parser, "centiloid", fullCommand);
}

int executeCenTauRCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(parser, "centaur", fullCommand);
}

int executeCenTauRzCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand) {
    return executeSUVrDerivedMetricCommand(parser, "centaurz", fullCommand);
}

int executeFillStatesCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand) {
    FillStatesCommandOptions options = parseFillStatesOptions(parser);

    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);

    ProcessingOptions procOptions;
    procOptions.skipRegistration = options.skipRegistration;
    procOptions.useIterativeRigid = options.useIterativeRigid;
    procOptions.useManualFOV = options.useManualFOV;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = options.metricType;
    procOptions.selectedMetricTracer = options.tracer;

    if (options.batchMode) {
        auto processor = [config, procOptions](const std::string& inputPath, const std::string& outputPath) -> ProcessingResult {
            ProcessingPipeline pipeline(config);
            return pipeline.process(inputPath, outputPath, procOptions);
        };

        std::cout << "Starting " << options.metricType << " batch processing..." << std::endl;
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

    ProcessingPipeline pipeline(config);
    std::cout << "Starting " << options.metricType << " calculation: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);

    std::cout << "\n=== " << options.metricType << " Results ===" << std::endl;

    for (const auto& metricResult : result.metricResults) {
        std::cout << "Metric: " << metricResult.metricName << std::endl;
        for (const auto& [tracer, value] : metricResult.tracerValues) {
            std::cout << tracer << ": " << value << std::endl;
        }
        std::cout << std::endl;
        if (options.includeSUVr) {
            std::cout << "SUVr: " << metricResult.suvr << std::endl << std::endl;
        }
    }

    std::cout << "Processing completed successfully!" << std::endl;
    return EXIT_SUCCESS;
}

int executeSUVrCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand) {
    SUVrCommandOptions options;
    options.inputPath = parser.get<std::string>("--input");
    options.outputPath = parser.get<std::string>("--output");
    options.voiMaskPath = parser.get<std::string>("--voi-mask");
    options.refMaskPath = parser.get<std::string>("--ref-mask");
    options.configPath = parser.get<std::string>("--config");
    options.skipRegistration = parser.get<bool>("--skip-normalization");
    options.enableDebugOutput = parser.get<bool>("--debug");
    options.batchMode = parser.get<bool>("--batch");
    
    setupDebugOutput(options);
    
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
                procOptions.selectedMetric = "";
                
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

    ImageType::Pointer inputImage;
    if (options.skipRegistration) {
        inputImage = Common::LoadNii(options.inputPath);
    } else {
        ProcessingOptions procOptions;
        procOptions.skipRegistration = false;
        procOptions.enableDebugOutput = options.enableDebugOutput;
        procOptions.debugOutputBasePath = options.debugOutputBasePath;
        procOptions.selectedMetric = "";
        
        ProcessingPipeline pipeline(config);
        ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
        inputImage = result.spatiallyNormalizedImage;
    }
    
    double suvr = SUVrCalculator::calculateSUVr(inputImage, options.voiMaskPath, options.refMaskPath);
    
    std::cout << "\n=== SUVr Results ===" << std::endl;
    std::cout << "VOI Mask: " << options.voiMaskPath << std::endl;
    std::cout << "Reference Mask: " << options.refMaskPath << std::endl;
    std::cout << "SUVr: " << suvr << std::endl;
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

int executeNormalizeCommand(const argparse::ArgumentParser& parser) {
    NormalizeCommandOptions options;
    options.inputPath = parser.get<std::string>("--input");
    options.outputPath = parser.get<std::string>("--output");
    options.configPath = parser.get<std::string>("--config");
    options.normalizationMethod = parser.get<std::string>("--method");
    options.useIterativeRigid = parser.get<bool>("--iterative");
    options.useManualFOV = parser.get<bool>("--manual-fov");
    options.enableADNIStyle = parser.get<bool>("--ADNI-PET-core");
    options.enableDebugOutput = parser.get<bool>("--debug");
    
    setupDebugOutput(options);
    
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    ProcessingOptions procOptions;
    procOptions.skipRegistration = false;
    procOptions.useIterativeRigid = options.useIterativeRigid;
    procOptions.useManualFOV = options.useManualFOV;
    procOptions.enableADNIStyle = options.enableADNIStyle;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = "";
    
    ProcessingPipeline pipeline(config);
    std::cout << "Starting spatial normalization: " << options.inputPath << std::endl;
    ProcessingResult result = pipeline.process(options.inputPath, options.outputPath, procOptions);
    
    std::cout << "\n=== Normalization Complete ===" << std::endl;
    std::cout << "Output image: " << options.outputPath << std::endl;
    std::cout << "Processing completed successfully!" << std::endl;
    
    return EXIT_SUCCESS;
}

int executeDecoupleCommand(const argparse::ArgumentParser& parser) {
    DecoupleCommandOptions options;
    options.inputPath = parser.get<std::string>("--input");
    options.outputPath = parser.get<std::string>("--output");
    options.modality = parser.get<std::string>("--modality");
    options.configPath = parser.get<std::string>("--config");
    options.skipRegistration = parser.get<bool>("--skip-normalization");
    options.enableDebugOutput = parser.get<bool>("--debug");
    
    setupDebugOutput(options);
    
    auto config = loadConfigurationWithLogging(options.configPath, options.enableDebugOutput);
    
    ProcessingOptions procOptions;
    procOptions.skipRegistration = options.skipRegistration;
    procOptions.decoupleModality = options.modality;
    procOptions.enableDebugOutput = options.enableDebugOutput;
    procOptions.debugOutputBasePath = options.debugOutputBasePath;
    procOptions.selectedMetric = "";
    
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

