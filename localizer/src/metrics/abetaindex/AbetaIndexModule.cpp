#include "AbetaIndexModule.h"

#include "AbetaIndexCLI.h"

namespace Pipeline::Metrics::AbetaIndex {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"abetaindex", true, &createCLI});
}

} // namespace Pipeline::Metrics::AbetaIndex
