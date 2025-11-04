#include "ProcessingPipeline.h"
#include "../factories/SpatialNormalizerFactory.h"
#include "../factories/MetricCalculatorFactory.h"
#include "../normalizers/RigidVoxelMorphNormalizer.h"
#include "../utils/common.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <map>

ProcessingPipeline::ProcessingPipeline(ConfigurationPtr config) : config_(config) {
    initializePipeline();
}

void ProcessingPipeline::initializePipeline() {
    // Create spatial normalizer
    spatialNormalizer_ = SpatialNormalizerFactory::createFromString("rigid_voxelmorph", config_);
    
    // Metric calculators will be created on-demand based on processing options
}

ProcessingResult ProcessingPipeline::process(const std::string& inputPath, 
                                           const std::string& outputPath,
                                           const ProcessingOptions& options) {
    ProcessingResult result;
    
    try {
        // 1. Spatial normalization
        if (!options.skipRegistration) {
            auto normalizationResult = performSpatialNormalization(inputPath, options);
            result.spatiallyNormalizedImage = normalizationResult.spatiallyNormalizedImage;
            result.rigidAlignedImage = normalizationResult.rigidAlignedImage;
        } else {
            ImageType::Pointer inputImage = loadAndValidateInput(inputPath);
            result.spatiallyNormalizedImage = inputImage;
            result.rigidAlignedImage = inputImage;  // Use input image as both if skipping registration
        }
        
        // Save spatial normalization result
        saveResult(result.spatiallyNormalizedImage, outputPath);
        
        // 2. Calculate semi-quantitative metrics
        if (!options.selectedMetric.empty()) {
            result.metricResults = calculateMetrics(result.spatiallyNormalizedImage, options.selectedMetric);
        }
        
        // 3. ADNI-style processing and decoupling (if needed)
        if (options.enableADNIStyle || !options.decoupleModality.empty()) {
            // Use the correctly rigid-aligned image to generate ADNI-style image
            ImageType::Pointer adniStyleImage = prepareADNIStyleImage(result.rigidAlignedImage, result.spatiallyNormalizedImage);
            
            saveResult(adniStyleImage, outputPath);
            
            // Execute decoupling
            if (!options.decoupleModality.empty()) {
                result.decoupledResult = performDecoupling(adniStyleImage, options.decoupleModality);
                result.hasDecoupledResult = true;
                result.decoupledResult.SaveResults(outputPath);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error occurred during processing: " << e.what() << std::endl;
        throw;
    }
    
    return result;
}

SpatialNormalizationResult ProcessingPipeline::performSpatialNormalization(const std::string& inputPath,
                                                                         const ProcessingOptions& options) {
    ImageType::Pointer inputImage = loadAndValidateInput(inputPath);
    SpatialNormalizationResult result;
    
    // Choose different normalization methods based on options
    auto rigidVoxelMorphNormalizer = std::dynamic_pointer_cast<RigidVoxelMorphNormalizer>(spatialNormalizer_);
    
    // Set debug options for normalizer
    if (rigidVoxelMorphNormalizer && options.enableDebugOutput) {
        rigidVoxelMorphNormalizer->setDebugMode(true, options.debugOutputBasePath);
    }
    
    if (options.useManualFOV) {
        // For manual FOV, use input image as rigid aligned (no rigid registration)
        result.rigidAlignedImage = inputImage;
        result.spatiallyNormalizedImage = rigidVoxelMorphNormalizer->normalizeManualFOV(inputImage);
    } else if (options.useIterativeRigid) {
        auto normResult = rigidVoxelMorphNormalizer->normalizeIterativeWithIntermediateResults(inputImage, 
                                                                                              options.maxIterations, 
                                                                                              options.convergenceThreshold);
        result.rigidAlignedImage = normResult.rigidAlignedImage;
        result.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
    } else {
        auto normResult = rigidVoxelMorphNormalizer->normalizeWithIntermediateResults(inputImage);
        result.rigidAlignedImage = normResult.rigidAlignedImage;
        result.spatiallyNormalizedImage = normResult.spatiallyNormalizedImage;
    }
    
    return result;
}

std::vector<MetricResult> ProcessingPipeline::calculateMetrics(ImageType::Pointer spatiallyNormalizedImage, 
                                                             const std::string& selectedMetric) {
    std::vector<MetricResult> results;
    
    // Create selected metric calculator
    auto calculators = MetricCalculatorFactory::createSelected(selectedMetric, config_);
    
    for (auto& calculator : calculators) {
        try {
            MetricResult result = calculator->calculate(spatiallyNormalizedImage);
            results.push_back(result);
        } catch (const std::exception& e) {
            std::cerr << "Error calculating metric " << calculator->getName() << ": " << e.what() << std::endl;
        }
    }
    
    return results;
}

DecoupledResult ProcessingPipeline::performDecoupling(ImageType::Pointer adniStyleImage, const std::string& modality) {
    std::string modelPath;
    std::string lowerModality = modality;
    std::transform(lowerModality.begin(), lowerModality.end(), lowerModality.begin(), ::tolower);
    
    if (lowerModality == "abeta") {
        modelPath = config_->getModelPath("abeta_decoupler");
    } else if (lowerModality == "tau") {
        modelPath = config_->getModelPath("tau_decoupler");
    } else {
        throw std::invalid_argument("Unsupported decoupling modality: " + modality);
    }

    // Prefer ensemble if provided
    std::vector<std::string> modelPaths;
    if (lowerModality == "abeta") {
        modelPaths = config_->getModelPaths("abeta_decoupler");
    } else if (lowerModality == "tau") {
        modelPaths = config_->getModelPaths("tau_decoupler");
    }

    DecoupledResult decoupled;
    if (!modelPaths.empty()) {
        Decoupler decoupler(modelPaths);
        decoupled = decoupler.predict(adniStyleImage);
    } else {
        Decoupler decoupler(modelPath);
        decoupled = decoupler.predict(adniStyleImage);
    }

    // Compute ADAD per-tracer using config conversion (slope * x + intercept)
    try {
        std::string sectionName = "adad_" + lowerModality + ".tracers";
        auto sec = config_->getSection(sectionName);
        // Aggregate slopes and intercepts by tracer name
        std::map<std::string, std::pair<float,float>> coeffs; // tracer -> (slope, intercept)
        for (const auto& kv : sec) {
            const std::string& key = kv.first; // e.g., "pib.slope"
            const std::string& val = kv.second; // string number
            auto dot = key.find('.');
            if (dot == std::string::npos) continue;
            std::string tracer = key.substr(0, dot);
            std::string what = key.substr(dot + 1);
            float fval = 0.0f;
            try { fval = std::stof(val); } catch (...) { continue; }
            auto &p = coeffs[tracer];
            if (what == "slope") p.first = fval; else if (what == "intercept") p.second = fval;
        }
        for (const auto& kv : coeffs) {
            const std::string& tracer = kv.first;
            float slope = kv.second.first;
            float intercept = kv.second.second;
            float converted = slope * static_cast<float>(decoupled.ADADscore) + intercept;
            decoupled.ADADTracerValues[tracer] = converted;
        }
    } catch (...) {
        // Silently ignore config parsing errors for ADAD printing
    }

    return decoupled;
}

ImageType::Pointer ProcessingPipeline::prepareADNIStyleImage(ImageType::Pointer rigidImage, 
                                                            ImageType::Pointer spatiallyNormalizedImage) {
    // Load cerebellar gray matter mask
    ImageType::Pointer refCerebralGray = Common::LoadNii(config_->getMaskPath("cerebral_gray"));
    
    // Resample spatially normalized image to mask space
    ImageType::Pointer resampledImage = Common::ResampleToMatch(refCerebralGray, spatiallyNormalizedImage);
    
    // Calculate cerebellar gray matter mean value
    double meanCerebralGray = Common::CalculateMeanInMask(resampledImage, refCerebralGray);
    
    // Load ADNI template
    ImageType::Pointer adniTemplate = Common::LoadNii(config_->getTemplatePath("adni_pet_core"));
    
    // Resample rigid-aligned image to ADNI template space
    ImageType::Pointer adniStyleImage = Common::ResampleToMatch(adniTemplate, rigidImage);
    
    // Perform intensity normalization
    Common::DivideVoxelsByValue(adniStyleImage, meanCerebralGray);
    
    return adniStyleImage;
}

ImageType::Pointer ProcessingPipeline::loadAndValidateInput(const std::string& inputPath) {
    ImageType::Pointer image = Common::LoadNii(inputPath);
    if (!image) {
        throw std::runtime_error("Unable to load input image: " + inputPath);
    }
    return image;
}

void ProcessingPipeline::saveResult(ImageType::Pointer image, const std::string& outputPath) {
    Common::SaveImage(image, outputPath);
}
