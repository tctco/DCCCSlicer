#include "ConfigLoader.h"
#include "Configuration.h"
#include <iostream>

namespace Pipeline {

namespace {

std::string resolvePath(const std::string& userProvided) {
    if (userProvided.empty()) {
        return "config.toml";
    }
    return Configuration::findConfigFile(userProvided);
}

std::string logPrefix(const std::string& tag) {
    if (tag.empty()) {
        return "[config]";
    }
    return "[" + tag + "]";
}

} // namespace

ConfigurationPtr loadConfigurationWithLogging(const ConfigurationLoadOptions& options) {
    auto configuration = std::make_shared<Configuration>();
    const std::string resolvedPath = resolvePath(options.configPath);
    const std::string prefix = logPrefix(options.logTag);

    bool loadSuccess = configuration->loadFromFile(resolvedPath);
    std::cout << prefix << " Loading configuration from: " << resolvedPath;
    if (loadSuccess) {
        std::cout << " [SUCCESS]" << std::endl;
    } else {
        std::cout << " [FAILED] - using default configuration" << std::endl;
        configuration->loadDefaults();
    }

    if (options.enableDebugOutput) {
        configuration->printAllConfigurations();
    }

    return configuration;
}

} // namespace Pipeline

