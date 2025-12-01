#include "Filesystem.h"

#include <algorithm>

namespace Common::fs {

bool ensureParentDirectory(const std::string& filePath) {
    auto directory = std::filesystem::path(filePath).parent_path();
    if (directory.empty()) {
        return true;
    }
    if (std::filesystem::exists(directory)) {
        return std::filesystem::is_directory(directory);
    }
    std::filesystem::create_directories(directory);
    return true;
}

bool ensureDirectory(const std::filesystem::path& dir) {
    if (std::filesystem::exists(dir)) {
        return std::filesystem::is_directory(dir);
    }
    std::filesystem::create_directories(dir);
    return true;
}

bool isDirectoryEmpty(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return true;
    }
    return std::filesystem::is_empty(dir);
}

bool isNiftiFile(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    if (ext == ".nii") {
        return true;
    }
    if (ext == ".gz") {
        auto stemExt = path.stem().extension().string();
        return stemExt == ".nii";
    }
    return false;
}

std::string baseNameFromNifti(const std::filesystem::path& file) {
    if (file.extension() == ".gz" && file.stem().extension() == ".nii") {
        return file.stem().stem().string();
    }
    return file.stem().string();
}

std::vector<std::filesystem::path> collectNiftiFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(directory)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && isNiftiFile(entry.path())) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string buildOutputPath(const std::filesystem::path& inputFile,
                            const std::filesystem::path& outputDir,
                            const std::string& suffix) {
    std::string baseName = baseNameFromNifti(inputFile);
    return (outputDir / (baseName + suffix)).string();
}

}  // namespace Common::fs


