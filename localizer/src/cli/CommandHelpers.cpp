#include "CommandHelpers.h"
#include "../core/config/ConfigLoader.h"

using RefactorPipeline::ConfigurationLoadOptions;

ConfigurationPtr loadConfigurationWithLogging(const std::string& configPath,
                                              bool debugMode) {
    ConfigurationLoadOptions options;
    options.configPath = configPath;
    options.enableDebugOutput = debugMode;
    options.logTag = "legacy-cli";
    return RefactorPipeline::loadConfigurationWithLogging(options);
}

