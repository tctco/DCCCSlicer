#pragma once
#include "../common/ProcessingContracts.h"
#include "../services/ISpatialNormalizationService.h"
#include "../services/IMetricService.h"
#include "../services/IFileService.h"
#include <functional>
#include <memory>

namespace RefactorPipeline {

using BatchSuccessCallback = std::function<void(const BatchProcessingItem&, const ProcessingResponse&)>;
using BatchErrorCallback = std::function<void(const BatchProcessingItem&, const std::exception&)>;

class PipelineApplication {
public:
    PipelineApplication(std::shared_ptr<ISpatialNormalizationService> spatialService,
                        std::shared_ptr<IMetricService> metricService,
                        std::shared_ptr<IFileService> fileService);

    ProcessingResponse run(const ProcessingRequest& request);
    BatchProcessingSummary runBatch(const BatchProcessingRequest& request,
                                    BatchSuccessCallback onSuccess = nullptr,
                                    BatchErrorCallback onError = nullptr);

private:
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IMetricService> metricService_;
    std::shared_ptr<IFileService> fileService_;
};

} // namespace RefactorPipeline

