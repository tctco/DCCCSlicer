#pragma once

#include <filesystem>
#include <string>

namespace Common::path {

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value);
std::string wideToUtf8(const std::wstring& value);
#endif

std::filesystem::path fromUtf8(const std::string& value);
std::string toUtf8(const std::filesystem::path& value);
std::string legacyFileName(const std::filesystem::path& value);
std::string addSuffix(const std::string& filePath, const std::string& suffix);
std::string toLower(std::string value);
std::string executableDirectory();
std::string deriveDebugBasePath(const std::string& outputPath);
void requireOutputDirectoryExists(const std::string& outputPath);

}  // namespace Common::path

