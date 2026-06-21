#include "ADADModule.h"

#include "ADADCLI.h"

namespace Pipeline::Metrics::ADAD {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"adad", true, &createCLI});
}

} // namespace Pipeline::Metrics::ADAD
