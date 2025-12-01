#pragma once

#include <string>

namespace Common::path {

std::string addSuffix(const std::string& filePath, const std::string& suffix);
std::string toLower(std::string value);
std::string executableDirectory();
std::string deriveDebugBasePath(const std::string& outputPath);

}  // namespace Common::path


