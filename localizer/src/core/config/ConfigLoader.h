#pragma once
#include "../interfaces/IConfiguration.h"
#include <string>

namespace Pipeline {

struct ConfigurationLoadOptions {
    std::string configPath;
    bool enableDebugOutput = false;
    std::string logTag;
};

ConfigurationPtr loadConfigurationWithLogging(const ConfigurationLoadOptions& options);

} // namespace Pipeline

