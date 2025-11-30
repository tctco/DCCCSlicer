#pragma once

#include <itkAffineTransform.h>
#include <itkImage.h>
#include <itkImageFileReader.h>

namespace refactorCommon::nifti {

using ImageType = itk::Image<float, 3>;
using DDFType = itk::Image<itk::Vector<double, 3>, 3>;
using BinaryImageType = itk::Image<unsigned char, 3>;
using ReaderType = itk::ImageFileReader<ImageType>;

}  // namespace refactorCommon::nifti


