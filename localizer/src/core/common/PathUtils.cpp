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
    return toUtf8(directory);
}

#ifdef _WIN32
std::wstring shortPathForExistingPath(const std::filesystem::path& path) {
    const std::wstring widePath = path.native();
    const DWORD length = GetShortPathNameW(widePath.c_str(), nullptr, 0);
    if (length == 0) {
        return {};
    }

    std::wstring buffer(length, L'\0');
    const DWORD written = GetShortPathNameW(widePath.c_str(), buffer.data(), length);
    if (written == 0) {
        return {};
    }
    buffer.resize(written);
    return buffer;
}
#endif

}  // namespace

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("Failed to convert UTF-8 path to UTF-16.");
    }

    std::wstring wide(length, L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), length);
    return wide;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        throw std::runtime_error("Failed to convert UTF-16 path to UTF-8.");
    }

    std::string utf8(length, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}
#endif

std::filesystem::path fromUtf8(const std::string& value) {
#ifdef _WIN32
    return std::filesystem::u8path(value);
#else
    return std::filesystem::path(value);
#endif
}

std::string toUtf8(const std::filesystem::path& value) {
#ifdef _WIN32
    return wideToUtf8(value.native());
#else
    return value.string();
#endif
}

std::string legacyFileName(const std::filesystem::path& value) {
#ifdef _WIN32
    if (const std::wstring shortPath = shortPathForExistingPath(value); !shortPath.empty()) {
        return wideToUtf8(shortPath);
    }

    const std::filesystem::path parent = value.parent_path();
    if (!parent.empty()) {
        if (const std::wstring shortParent = shortPathForExistingPath(parent); !shortParent.empty()) {
            return wideToUtf8((std::filesystem::path(shortParent) / value.filename()).native());
        }
    }
#endif
    return toUtf8(value);
}

std::string addSuffix(const std::string& filePath, const std::string& suffix) {
    std::filesystem::path path = fromUtf8(filePath);
    auto directory = path.parent_path();
    auto stem = toUtf8(path.stem());
    auto extension = toUtf8(path.extension());

    std::filesystem::path updated = directory / fromUtf8(stem + suffix + extension);
    return toUtf8(updated);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string executableDirectory() {
    std::string executablePath;

#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0) {
        buffer.resize(length);
        executablePath = wideToUtf8(buffer);
    }
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
        executablePath = toUtf8(std::filesystem::current_path());
    }
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    executablePath = std::string(result, (count > 0) ? count : 0);
#endif

    return toUtf8(fromUtf8(executablePath).parent_path());
}

std::string deriveDebugBasePath(const std::string& outputPath) {
    if (outputPath.empty()) {
        return {};
    }
    std::filesystem::path filePath = fromUtf8(outputPath);
    std::string directory = normalizeDirectory(filePath.parent_path());
    std::string baseName = toUtf8(filePath.stem());
    if (directory.empty()) {
        return baseName;
    }
    return directory + "/" + baseName;
}

void requireOutputDirectoryExists(const std::string& outputPath) {
    if (outputPath.empty()) {
        throw std::invalid_argument("Output path must not be empty");
    }
    std::filesystem::path directory = fromUtf8(outputPath).parent_path();
    if (directory.empty()) {
        return;
    }
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error("Output directory does not exist: " + toUtf8(directory));
    }
}

}  // namespace Common::path
