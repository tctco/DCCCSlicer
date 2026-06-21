#pragma once

#include "../common/NormalizationContracts.h"

namespace Pipeline {

class ISpatialNormalizationService {
public:
    virtual ~ISpatialNormalizationService() = default;

    virtual SpatialNormalizationOutput normalize(const SpatialNormalizationRequest& request) = 0;
};

} // namespace Pipeline
