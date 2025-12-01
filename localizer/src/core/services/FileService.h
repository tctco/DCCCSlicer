#pragma once
#include "IFileService.h"

namespace Pipeline {

class FileService : public IFileService {
public:
    FileService() = default;
    ~FileService() override = default;

    void saveNormalizedImage(const FileSaveRequest& request) override;
};

} // namespace Pipeline

