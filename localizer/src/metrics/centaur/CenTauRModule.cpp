#include "CenTauRModule.h"

#include "CenTauRCLI.h"

namespace Pipeline::Metrics::Centaur {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"centaur", true, &createCLI});
}

} // namespace Pipeline::Metrics::Centaur
