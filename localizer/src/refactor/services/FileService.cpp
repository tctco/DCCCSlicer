#include "FileService.h"
#include "../../utils/common.h"
#include <stdexcept>

namespace RefactorPipeline {

void FileService::saveNormalizedImage(const FileSaveRequest& request) {
    if (!request.spatiallyNormalizedImage) {
        throw std::invalid_argument("FileService requires a spatially normalized image to save");
    }
    if (request.outputPath.empty()) {
        throw std::invalid_argument("FileService requires a valid output path");
    }

    Common::SaveImage(request.spatiallyNormalizedImage, request.outputPath);
}

} // namespace RefactorPipeline

