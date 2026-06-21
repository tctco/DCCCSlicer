#include "ModuleCatalog.h"
#include "standard/NormalizeCLI.h"
#include "adni/AdniPetCoreCLI.h"
#include "rigid/RigidCLI.h"

namespace Pipeline::SpatialNormalization {

std::vector<SpatialNormalizationCLIPtr> buildCLIModules() {
    std::vector<SpatialNormalizationCLIPtr> modules;
    modules.push_back(Standard::createCLI());
    modules.push_back(Adni::createCLI());
    modules.push_back(Rigid::createCLI());
    return modules;
}

} // namespace Pipeline::SpatialNormalization


