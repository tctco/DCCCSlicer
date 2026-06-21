#pragma once

#include <memory>
#include <string>
#include "ServiceContainer.h"

namespace Pipeline {

struct BootstrapOptions {
    std::string configPath;
    bool enableConfigDebug = false;
    std::string logTag;
};

std::shared_ptr<ServiceContainer> buildCoreContainer(const BootstrapOptions& options);

} // namespace Pipeline
