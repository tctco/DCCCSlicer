#include "PathUtils.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <unistd.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace Common::path {

namespace {

std::string normalizeDirectory(const std::filesystem::path& directory) {
    if (directory.empty()) {
        return {};
    }
    return directory.string();
}

}  // namespace

std::string addSuffix(const std::string& filePath, const std::string& suffix) {
    std::filesystem::path path(filePath);
    auto directory = path.parent_path();
    auto stem = path.stem().string();
    auto extension = path.extension().string();

    std::filesystem::path updated = directory / (stem + suffix + extension);
    return updated.string();
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string executableDirectory() {
    std::string executablePath;

#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    executablePath = buffer;
#elif __APPLE__
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        char resolvedPath[PATH_MAX];
        if (realpath(path, resolvedPath) != nullptr) {
            executablePath = resolvedPath;
        }
    }
    if (executablePath.empty()) {
        executablePath = std::filesystem::current_path().string();
    }
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    executablePath = std::string(result, (count > 0) ? count : 0);
#endif

    return std::filesystem::path(executablePath).parent_path().string();
}

std::string deriveDebugBasePath(const std::string& outputPath) {
    if (outputPath.empty()) {
        return {};
    }
    std::filesystem::path filePath(outputPath);
    std::string directory = normalizeDirectory(filePath.parent_path());
    std::string baseName = filePath.stem().string();
    if (directory.empty()) {
        return baseName;
    }
    return directory + "/" + baseName;
}

void requireOutputDirectoryExists(const std::string& outputPath) {
    if (outputPath.empty()) {
        throw std::invalid_argument("Output path must not be empty");
    }
    std::filesystem::path directory = std::filesystem::path(outputPath).parent_path();
    if (directory.empty()) {
        return;
    }
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error("Output directory does not exist: " + directory.string());
    }
}

}  // namespace Common::path


