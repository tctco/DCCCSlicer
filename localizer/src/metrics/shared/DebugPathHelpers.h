#pragma once
#include <string>

#include "../../core/common/PathUtils.h"

namespace Pipeline::Metrics::Shared {

template <typename Options>
void configureDerivedDebugBasePath(Options& options) {
    if (!options.enableDebugOutput || options.outputPath.empty()) {
        options.debugOutputBasePath.clear();
        return;
    }
    options.debugOutputBasePath = Common::path::deriveDebugBasePath(options.outputPath);
}

} // namespace Pipeline::Metrics::Shared

