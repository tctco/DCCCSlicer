#pragma once
#include <vector>
#include "../core/interfaces/ISpatialNormalizationCLI.h"

namespace RefactorPipeline::SpatialNormalization {

std::vector<SpatialNormalizationCLIPtr> buildCLIModules();

} // namespace RefactorPipeline::SpatialNormalization



