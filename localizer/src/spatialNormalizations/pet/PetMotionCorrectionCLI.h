#pragma once

#include "../../core/interfaces/ISpatialNormalizationCLI.h"

namespace Pipeline::SpatialNormalization::Pet {

SpatialNormalizationCLIPtr createMotionCorrectionCLI();

} // namespace Pipeline::SpatialNormalization::Pet
