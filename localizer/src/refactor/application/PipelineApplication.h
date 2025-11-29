#pragma once
#include "../common/ProcessingContracts.h"
#include "../services/ISpatialNormalizationService.h"
#include "../services/IMetricService.h"
#include "../services/IFileService.h"
#include <memory>

namespace RefactorPipeline {

class PipelineApplication {
public:
    PipelineApplication(std::shared_ptr<ISpatialNormalizationService> spatialService,
                        std::shared_ptr<IMetricService> metricService,
                        std::shared_ptr<IFileService> fileService);

    ProcessingResponse run(const ProcessingRequest& request);

private:
    std::shared_ptr<ISpatialNormalizationService> spatialService_;
    std::shared_ptr<IMetricService> metricService_;
    std::shared_ptr<IFileService> fileService_;
};

} // namespace RefactorPipeline

