#include "CentaurzModule.h"

#include "CentaurzCLI.h"

namespace Pipeline::Metrics::Centaurz {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"centaurz", true, &createCLI});
}

} // namespace Pipeline::Metrics::Centaurz
