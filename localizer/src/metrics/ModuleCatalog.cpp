#include "ModuleCatalog.h"
#include "abetaindex/AbetaIndexModule.h"
#include "abetaload/AbetaLoadModule.h"
#include "adad/ADADModule.h"
#include "centaur/CenTauRModule.h"
#include "centaurz/CentaurzModule.h"
#include "fillstates/FillStatesModule.h"
#include "list/MetricsCLI.h"
#include "centiloid/CentiloidModule.h"
#include "suvr/SUVrModule.h"
#include "shared/MetricRegistry.h"

namespace Pipeline::Metrics {

namespace {

MetricRegistry buildRegistry() {
    MetricRegistry registry;
    List::registerModule(registry);
    SUVr::registerModule(registry);
    Centiloid::registerModule(registry);
    Centaur::registerModule(registry);
    Centaurz::registerModule(registry);
    FillStates::registerModule(registry);
    ADAD::registerModule(registry);
    AbetaIndex::registerModule(registry);
    AbetaLoad::registerModule(registry);
    return registry;
}

} // namespace

std::vector<MetricCLIPtr> buildCLIModules() {
    return buildRegistry().createCLIModules();
}

std::vector<std::string> listMetricNames() {
    return buildRegistry().listMetricNames();
}

} // namespace Pipeline::Metrics
