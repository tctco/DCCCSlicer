#include "ModuleCatalog.h"
#include "centiloid/CentiloidCLI.h"
#include "centiloid/CentiloidLogic.h"
#include "centaur/CenTauRCLI.h"
#include "centaur/CenTauRLogic.h"
#include "centaurz/CentaurzCLI.h"
#include "centaurz/CentaurzLogic.h"
#include "fillstates/FillStatesCLI.h"
#include "fillstates/FillStatesLogic.h"
#include "suvr/SUVrCLI.h"
#include "suvr/SUVrLogic.h"
#include "adad/ADADCLI.h"
#include "adad/ADADLogic.h"

namespace RefactorPipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules() {
    std::vector<MetricCLIPtr> modules;
    modules.push_back(SUVr::createCLI());
    modules.push_back(Centiloid::createCLI());
    modules.push_back(Centaur::createCLI());
    modules.push_back(Centaurz::createCLI());
    modules.push_back(FillStates::createCLI());
    modules.push_back(ADAD::createCLI());
    return modules;
}

void registerAllMetricModules(ServiceContainer& container) {
    SUVr::registerMetric(container);
    Centiloid::registerMetric(container);
    Centaur::registerMetric(container);
    Centaurz::registerMetric(container);
    FillStates::registerMetric(container);
    ADAD::registerMetric(container);
}

} // namespace RefactorPipeline::Metrics

