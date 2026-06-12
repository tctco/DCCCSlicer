#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Common::fs {

bool ensureParentDirectory(const std::string& filePath);
bool ensureDirectory(const std::filesystem::path& dir);
bool isDirectoryEmpty(const std::filesystem::path& dir);
bool isNiftiFile(const std::filesystem::path& path);
bool isBidsPetNiftiFile(const std::filesystem::path& path);
std::string baseNameFromNifti(const std::filesystem::path& file);
std::vector<std::filesystem::path> collectNiftiFiles(const std::filesystem::path& directory);
std::vector<std::filesystem::path> collectBidsPetNiftiFiles(const std::filesystem::path& datasetRoot,
                                                            const std::string& pattern);
std::vector<std::filesystem::path> collectInputNiftiFiles(const std::filesystem::path& directory,
                                                          const std::string& bidsPattern);
std::string buildOutputPath(const std::filesystem::path& inputFile,
                            const std::filesystem::path& outputDir,
                            const std::string& suffix);

}  // namespace Common::fs
