#include "Filesystem.h"

#include "PathUtils.h"
#include <algorithm>
#include <regex>

namespace Common::fs {

bool ensureParentDirectory(const std::string& filePath) {
    auto directory = Common::path::fromUtf8(filePath).parent_path();
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
    auto ext = Common::path::toLower(Common::path::toUtf8(path.extension()));
    if (ext == ".nii") {
        return true;
    }
    if (ext == ".gz") {
        auto stemExt = Common::path::toLower(Common::path::toUtf8(path.stem().extension()));
        return stemExt == ".nii";
    }
    return false;
}

bool isBidsPetNiftiFile(const std::filesystem::path& path) {
    if (!isNiftiFile(path)) {
        return false;
    }

    bool inPetDirectory = false;
    for (const auto& part : path.parent_path()) {
        if (Common::path::toLower(Common::path::toUtf8(part)) == "pet") {
            inPetDirectory = true;
            break;
        }
    }
    if (!inPetDirectory) {
        return false;
    }

    const std::string baseName = baseNameFromNifti(path);
    return baseName.rfind("sub-", 0) == 0
        && baseName.size() >= 4
        && baseName.rfind("_pet") == baseName.size() - 4;
}

std::string baseNameFromNifti(const std::filesystem::path& file) {
    if (file.extension() == ".gz" && file.stem().extension() == ".nii") {
        return Common::path::toUtf8(file.stem().stem());
    }
    return Common::path::toUtf8(file.stem());
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

std::vector<std::filesystem::path> collectBidsPetNiftiFiles(const std::filesystem::path& datasetRoot,
                                                            const std::string& pattern) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(datasetRoot)) {
        return files;
    }

    const std::regex nameFilter(pattern);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(datasetRoot)) {
        if (!entry.is_regular_file() || !isBidsPetNiftiFile(entry.path())) {
            continue;
        }

        const std::string relativePath =
            Common::path::toUtf8(std::filesystem::relative(entry.path(), datasetRoot));
        const std::string filename = Common::path::toUtf8(entry.path().filename());
        const std::string baseName = baseNameFromNifti(entry.path());
        if (std::regex_search(relativePath, nameFilter)
            || std::regex_search(filename, nameFilter)
            || std::regex_search(baseName, nameFilter)) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<std::filesystem::path> collectInputNiftiFiles(const std::filesystem::path& directory,
                                                          const std::string& bidsPattern) {
    return bidsPattern.empty()
        ? collectNiftiFiles(directory)
        : collectBidsPetNiftiFiles(directory, bidsPattern);
}

std::string buildOutputPath(const std::filesystem::path& inputFile,
                            const std::filesystem::path& outputDir,
                            const std::string& suffix) {
    std::string baseName = baseNameFromNifti(inputFile);
    return Common::path::toUtf8(outputDir / Common::path::fromUtf8(baseName + suffix));
}

}  // namespace Common::fs
