#include "SUVrModule.h"

#include "SUVrCLI.h"

namespace Pipeline::Metrics::SUVr {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"suvr", true, &createCLI});
}

} // namespace Pipeline::Metrics::SUVr
