#include "FillStatesModule.h"

#include "FillStatesCLI.h"

namespace Pipeline::Metrics::FillStates {

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"fillstates", true, &createCLI});
}

} // namespace Pipeline::Metrics::FillStates
