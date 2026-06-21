#include "AbetaLoadModule.h"

#include "AbetaLoadCLI.h"

namespace Pipeline::Metrics::AbetaLoad {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"abetaload", true, &createCLI});
}

} // namespace Pipeline::Metrics::AbetaLoad
