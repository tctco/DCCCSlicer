#pragma once
#include "../common/ProcessingContracts.h"

namespace Pipeline {

class ISpatialNormalizationService {
public:
    virtual ~ISpatialNormalizationService() = default;

    virtual SpatialNormalizationOutput normalize(const SpatialNormalizationRequest& request) = 0;
};

} // namespace Pipeline

