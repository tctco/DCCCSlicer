#pragma once

#include "../common/NormalizationContracts.h"

namespace Pipeline {

class IFileService {
public:
    virtual ~IFileService() = default;

    virtual void saveNormalizedImage(const FileSaveRequest& request) = 0;
};

} // namespace Pipeline
