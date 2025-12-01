#pragma once
#include "../core/interfaces/IMetricLogic.h"
#include "../core/di/ServiceContainer.h"

namespace Pipeline::SpatialNormalization {

inline constexpr const char* kNullMetricName = "__spatial_normalization_null";

void registerNullMetric(ServiceContainer& container);

} // namespace Pipeline::SpatialNormalization

