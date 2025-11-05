#pragma once

#include <string>
#include "onnxruntime_cxx_api.h"

namespace OrtUtils {
inline std::basic_string<ORTCHAR_T> MakeOrtPath(const std::string& path) {
#ifdef _WIN32
    return std::wstring(path.begin(), path.end());
#else
    return path;
#endif
}
}  // namespace OrtUtils

