#pragma once

#include "PathUtils.h"
#include <filesystem>
#include <functional>
#include <type_traits>
#include <string>

#include "onnxruntime_cxx_api.h"

namespace Common::onnx {

namespace detail {

inline bool containsNonAscii(const std::string& value) {
    for (const unsigned char ch : value) {
        if (ch > 0x7F) {
            return true;
        }
    }
    return false;
}

inline std::string stageModelPathIfNeeded(const std::string& path) {
#ifdef _WIN32
    if (containsNonAscii(path)) {
        const std::filesystem::path sourcePath = Common::path::fromUtf8(path);
        const std::filesystem::path tempDir =
            std::filesystem::temp_directory_path() / "dcccslicer_onnx_models";
        std::filesystem::create_directories(tempDir);

        const std::string extension = sourcePath.has_extension()
            ? Common::path::toUtf8(sourcePath.extension())
            : std::string{".onnx"};
        const std::string fileName =
            "model_" + std::to_string(std::hash<std::string>{}(path)) + extension;
        const std::filesystem::path stagedPath = tempDir / fileName;

        std::filesystem::copy_file(
            sourcePath, stagedPath, std::filesystem::copy_options::overwrite_existing);
        return Common::path::legacyFileName(stagedPath);
    }
#endif
    return path;
}

} // namespace detail

inline std::basic_string<ORTCHAR_T> makeOrtPath(const std::string& path) {
    const std::string preparedPath = detail::stageModelPathIfNeeded(path);

#ifdef _WIN32
    if constexpr (std::is_same_v<ORTCHAR_T, wchar_t>) {
        return Common::path::utf8ToWide(preparedPath);
    } else {
        return preparedPath;
    }
#else
    return preparedPath;
#endif
}

}  // namespace Common::onnx
