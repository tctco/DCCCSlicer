#pragma once
#include <string>
#include "../core/interfaces/IConfiguration.h"

/**
 * Helper utilities reused by CLI modules.
 */
ConfigurationPtr loadConfigurationWithLogging(const std::string& configPath,
                                              bool debugMode = false);

