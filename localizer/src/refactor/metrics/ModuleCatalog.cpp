#include "ModuleCatalog.h"
#include "suvr/SUVrCLI.h"
#include "centiloid/CentiloidCLI.h"

namespace RefactorPipeline::Metrics {

std::vector<MetricCLIPtr> buildCLIModules() {
    std::vector<MetricCLIPtr> modules;
    modules.push_back(SUVr::createCLI());
    modules.push_back(Centiloid::createCLI());
    return modules;
}

} // namespace RefactorPipeline::Metrics

