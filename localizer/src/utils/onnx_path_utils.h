#pragma once

#include <string>
#include "../refactor/common/OnnxPath.h"

namespace OrtUtils {
inline std::basic_string<ORTCHAR_T> MakeOrtPath(const std::string& path) {
    return refactorCommon::onnx::makeOrtPath(path);
}
}  // namespace OrtUtils

