#include "FileService.h"
#include "../common/Common.h"
#include <stdexcept>

namespace RefactorPipeline {

void FileService::saveNormalizedImage(const FileSaveRequest& request) {
    if (!request.spatiallyNormalizedImage) {
        throw std::invalid_argument("FileService requires a spatially normalized image to save");
    }
    if (request.outputPath.empty()) {
        throw std::invalid_argument("FileService requires a valid output path");
    }

    refactorCommon::nifti::saveImage(request.spatiallyNormalizedImage, request.outputPath);
}

} // namespace RefactorPipeline

