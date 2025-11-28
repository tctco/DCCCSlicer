#pragma once
#include "../interfaces/ISpatialNormalizer.h"
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"
#include "../decouplers/Decoupler.h"
#include <vector>

/**
 * @brief Spatial normalization result
 */
struct SpatialNormalizationResult {
    ImageType::Pointer rigidAlignedImage;
    ImageType::Pointer spatiallyNormalizedImage;
};

/**
 * @brief Processing pipeline options
 */
struct ProcessingOptions {
    bool skipRegistration = false;
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableADNIStyle = false;
    std::string decoupleModality = "";  // "abeta", "tau" or empty
    
    // Iteration parameters
    int maxIterations = 5;
    float convergenceThreshold = 2.0f;
    
    // Debug parameters
    bool enableDebugOutput = false;
    std::string debugOutputBasePath = "";
    
    // Metric selection parameters
    std::string selectedMetric = "";         // "suvr", "centiloid", "centaur", "centaurz", "fillstates"
    std::string selectedMetricTracer = "";   // "fbp", "fdg", "ftp" (for tracer-dependent metrics)
};

/**
 * @brief Processing result
 */
struct ProcessingResult {
    ImageType::Pointer spatiallyNormalizedImage;
    ImageType::Pointer rigidAlignedImage;  // Add rigid-aligned intermediate result
    std::vector<MetricResult> metricResults;
    DecoupledResult decoupledResult;
    bool hasDecoupledResult = false;
    ImageType::Pointer fillStatesMaskImage;  // Optional fill-states mask
    bool hasFillStatesMask = false;
    
    void printAllResults() const {
        for (const auto& result : metricResults) {
            result.printResult();
        }
        if (hasDecoupledResult) {
            decoupledResult.printResult();
        }
    }
};

/**
 * @brief Main processing pipeline
 */
class ProcessingPipeline {
public:
    explicit ProcessingPipeline(ConfigurationPtr config);
    virtual ~ProcessingPipeline() = default;
    
    /**
     * @brief Execute complete processing workflow
     */
    ProcessingResult process(const std::string& inputPath, 
                           const std::string& outputPath,
                           const ProcessingOptions& options = ProcessingOptions{});
    
    /**
     * @brief Execute spatial normalization only
     */
    SpatialNormalizationResult performSpatialNormalization(const std::string& inputPath,
                                                           const ProcessingOptions& options);
    
    /**
     * @brief Execute metric calculation only
     */
    std::vector<MetricResult> calculateMetrics(ImageType::Pointer spatiallyNormalizedImage,
                                               const ProcessingOptions& options,
                                               ImageType::Pointer& fillStatesMaskOut);
    
    /**
     * @brief Execute decoupling analysis
     */
    DecoupledResult performDecoupling(ImageType::Pointer adniStyleImage, const std::string& modality);
    
private:
    ConfigurationPtr config_;
    SpatialNormalizerPtr spatialNormalizer_;
    
    void initializePipeline();
    ImageType::Pointer loadAndValidateInput(const std::string& inputPath);
    void saveResult(ImageType::Pointer image, const std::string& outputPath);
    ImageType::Pointer prepareADNIStyleImage(ImageType::Pointer rigidImage, ImageType::Pointer spatiallyNormalizedImage);
};

