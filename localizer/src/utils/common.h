#pragma once
#ifndef COMMON_H
#define COMMON_H
#include <itkAffineTransform.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkLabelStatisticsImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkWarpImageFilter.h>

#include <filesystem>
#include <string>

using ImageType = itk::Image<float, 3>;
using DDFType = itk::Image<itk::Vector<double, 3>, 3>;
using BinaryImageType = itk::Image<unsigned char, 3>;
using ReaderType = itk::ImageFileReader<ImageType>;

namespace Common {

void SaveImage(ImageType::Pointer image, const std::string& filename);
ImageType::Pointer LoadNii(const std::string& filename);
void DivideVoxelsByValue(ImageType::Pointer image, float divisor);
double CalculateMeanInMask(ImageType::Pointer image, ImageType::Pointer mask);
ImageType::Pointer ResampleToMatch(typename ImageType::Pointer referenceImage,
                                   typename ImageType::Pointer inputImage);
ImageType::Pointer CreateImageFromVector(const std::vector<float>& imageData,
                                         ImageType::SizeType size);
void ExtractImageData(ImageType::Pointer image, std::vector<float>& imageData);
std::string addSuffixToFilePath(const std::string& filePath,
                                const std::string& suffix);
void debugLog(const std::string& message);
std::string toLower(const std::string& str);
std::string getExecutablePath();
}  // namespace Common

#endif
