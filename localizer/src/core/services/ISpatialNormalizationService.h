#pragma once
#include "../common/ProcessingContracts.h"

namespace RefactorPipeline {

class ISpatialNormalizationService {
public:
    virtual ~ISpatialNormalizationService() = default;

    virtual SpatialNormalizationOutput normalize(const SpatialNormalizationRequest& request) = 0;
};

} // namespace RefactorPipeline

