#pragma once
#include "../../core/interfaces/ISpatialNormalizationCLI.h"

namespace Pipeline::SpatialNormalization::Adni {

SpatialNormalizationCLIPtr createCLI();
SpatialNormalizationCLIPtr createScanCLI();

} // namespace Pipeline::SpatialNormalization::Adni



