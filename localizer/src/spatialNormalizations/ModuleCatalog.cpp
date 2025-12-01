#include "ModuleCatalog.h"
#include "standard/NormalizeCLI.h"
#include "adni/AdniPetCoreCLI.h"

namespace RefactorPipeline::SpatialNormalization {

std::vector<SpatialNormalizationCLIPtr> buildCLIModules() {
    std::vector<SpatialNormalizationCLIPtr> modules;
    modules.push_back(Standard::createCLI());
    modules.push_back(Adni::createCLI());
    return modules;
}

} // namespace RefactorPipeline::SpatialNormalization



