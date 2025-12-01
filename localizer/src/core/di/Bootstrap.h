#pragma once
#include <memory>
#include "ServiceContainer.h"
#include "../application/PipelineApplication.h"

namespace RefactorPipeline {

struct BootstrapOptions {
    std::string configPath;
    bool enableConfigDebug = false;
    std::string logTag;
};

std::shared_ptr<ServiceContainer> buildDefaultContainer(const BootstrapOptions& options);
std::shared_ptr<PipelineApplication> resolveApplication(ServiceContainer& container);

} // namespace RefactorPipeline

