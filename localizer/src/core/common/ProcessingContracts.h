#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "../interfaces/IMetricCalculator.h"
#include "Common.h"

using ImageType = Common::ImageType;

namespace Pipeline {

struct SpatialNormalizationOptions {
    bool useIterativeRigid = false;
    bool useManualFOV = false;
    bool enableDebugOutput = false;
    std::string debugOutputBasePath;
    bool enableAdniPetCore = false;
    int maxIterations = 5;
    float convergenceThreshold = 2.0f;
};

struct SpatialNormalizationRequest {
    std::string inputPath;
    bool skip = false;
    SpatialNormalizationOptions options;
};

struct SpatialNormalizationOutput {
    ImageType::Pointer rigidAlignedImage;
    ImageType::Pointer spatiallyNormalizedImage;
};

struct MetricComputationOptions {
    std::string metricName;
    std::unordered_map<std::string, std::string> stringParameters;
};

struct MetricComputationRequest {
    ImageType::Pointer spatiallyNormalizedImage;
    ImageType::Pointer rigidAlignedImage;
    MetricComputationOptions options;
};

struct FileSaveRequest {
    ImageType::Pointer spatiallyNormalizedImage;
    std::string outputPath;
};

struct ProcessingRequest {
    SpatialNormalizationRequest normalization;
    std::string outputPath;
    bool persistNormalizedImage = true;
    bool computeMetrics = true;
    MetricComputationOptions metricOptions;
};

struct ProcessingResponse {
    SpatialNormalizationOutput normalizationOutput;
    std::vector<MetricResult> metricResults;
};

struct BatchProcessingItem {
    ProcessingRequest request;
    std::string label;
};

struct BatchProcessingRequest {
    std::vector<BatchProcessingItem> items;
};

struct BatchProcessingSummary {
    size_t processed = 0;
    size_t succeeded = 0;
    size_t failed = 0;
};

} // namespace Pipeline

