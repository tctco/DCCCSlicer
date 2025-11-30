#pragma once

#include <string>

#include "onnxruntime_cxx_api.h"

namespace refactorCommon::onnx {

inline std::basic_string<ORTCHAR_T> makeOrtPath(const std::string& path) {
#ifdef _WIN32
    return std::wstring(path.begin(), path.end());
#else
    return path;
#endif
}

}  // namespace refactorCommon::onnx


