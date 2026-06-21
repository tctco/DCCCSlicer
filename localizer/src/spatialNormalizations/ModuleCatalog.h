#pragma once
#include <vector>
#include "../core/interfaces/ISpatialNormalizationCLI.h"

namespace Pipeline::SpatialNormalization {

std::vector<SpatialNormalizationCLIPtr> buildCLIModules();

} // namespace Pipeline::SpatialNormalization



