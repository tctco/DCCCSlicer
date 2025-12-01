#include "PipelineApplication.h"
#include <stdexcept>

namespace Pipeline {

PipelineApplication::PipelineApplication(std::shared_ptr<ISpatialNormalizationService> spatialService,
                                         std::shared_ptr<IMetricService> metricService,
                                         std::shared_ptr<IFileService> fileService)
    : spatialService_(std::move(spatialService)),
      metricService_(std::move(metricService)),
      fileService_(std::move(fileService)) {
    if (!spatialService_ || !metricService_ || !fileService_) {
        throw std::invalid_argument("PipelineApplication requires all services to be non-null");
    }
}

ProcessingResponse PipelineApplication::run(const ProcessingRequest& request) {
    ProcessingResponse response;

    response.normalizationOutput = spatialService_->normalize(request.normalization);

    if (request.persistNormalizedImage) {
        FileSaveRequest saveRequest;
        saveRequest.spatiallyNormalizedImage = response.normalizationOutput.spatiallyNormalizedImage;
        saveRequest.outputPath = request.outputPath;
        fileService_->saveNormalizedImage(saveRequest);
    }

    if (request.computeMetrics && !request.metricOptions.metricName.empty()) {
        MetricComputationRequest metricRequest;
        metricRequest.spatiallyNormalizedImage = response.normalizationOutput.spatiallyNormalizedImage;
        metricRequest.rigidAlignedImage = response.normalizationOutput.rigidAlignedImage;
        metricRequest.options = request.metricOptions;
        response.metricResults = metricService_->calculate(metricRequest);
    }

    return response;
}

BatchProcessingSummary PipelineApplication::runBatch(const BatchProcessingRequest& request,
                                                     BatchSuccessCallback onSuccess,
                                                     BatchErrorCallback onError) {
    BatchProcessingSummary summary;
    for (const auto& item : request.items) {
        summary.processed++;
        try {
            auto response = run(item.request);
            summary.succeeded++;
            if (onSuccess) {
                onSuccess(item, response);
            }
        } catch (const std::exception& ex) {
            summary.failed++;
            if (onError) {
                onError(item, ex);
            }
        }
    }
    return summary;
}

} // namespace Pipeline

