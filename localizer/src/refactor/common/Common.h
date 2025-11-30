#pragma once

#include "NiftiTypes.h"
#include "NiftiIO.h"
#include "ImageOps.h"
#include "PathUtils.h"
#include "Filesystem.h"
#include "Logging.h"
#include "OnnxPath.h"

namespace refactorCommon {

// Convenience aliases for frequent consumers.
using ImageType = nifti::ImageType;
using DDFType = nifti::DDFType;
using BinaryImageType = nifti::BinaryImageType;
using ReaderType = nifti::ReaderType;

}  // namespace refactorCommon


