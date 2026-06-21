#include "CentiloidModule.h"

#include "CentiloidCLI.h"

namespace Pipeline::Metrics::Centiloid {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"centiloid", true, &createCLI});
}

} // namespace Pipeline::Metrics::Centiloid
