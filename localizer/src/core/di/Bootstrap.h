#pragma once
#include <memory>
#include "../../interfaces/IConfiguration.h"
#include "ServiceContainer.h"
#include "../application/PipelineApplication.h"

namespace RefactorPipeline {

std::shared_ptr<ServiceContainer> buildDefaultContainer(ConfigurationPtr config);
std::shared_ptr<PipelineApplication> resolveApplication(ServiceContainer& container);

} // namespace RefactorPipeline

