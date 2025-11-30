#include "ImageOps.h"

#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkLabelStatisticsImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkWarpImageFilter.h>

#include <iostream>

namespace refactorCommon::image {

void divideVoxelsByValue(ImageType::Pointer image, float divisor) {
    itk::ImageRegionIterator<ImageType> it(
        image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        it.Set(it.Get() / divisor);
    }
}

double calculateMeanInMask(ImageType::Pointer image,
                           ImageType::Pointer mask) {
    using LabelStatisticsFilterType =
        itk::LabelStatisticsImageFilter<ImageType, ImageType>;
    LabelStatisticsFilterType::Pointer labelStatisticsFilter = LabelStatisticsFilterType::New();

    labelStatisticsFilter->SetInput(image);
    labelStatisticsFilter->SetLabelInput(mask);
    labelStatisticsFilter->Update();

    const unsigned char maskLabel = 1;
    if (labelStatisticsFilter->HasLabel(maskLabel)) {
        return labelStatisticsFilter->GetMean(maskLabel);
    }

    std::cerr << "Mask does not contain the specified label." << std::endl;
    return 0.0;
}

ImageType::Pointer resampleToMatch(ImageType::Pointer referenceImage,
                                   ImageType::Pointer inputImage) {
    using ResampleFilterType = itk::ResampleImageFilter<ImageType,
                                                       ImageType>;
    ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();

    resampleFilter->SetInput(inputImage);
    resampleFilter->SetSize(referenceImage->GetLargestPossibleRegion().GetSize());
    resampleFilter->SetOutputSpacing(referenceImage->GetSpacing());
    resampleFilter->SetOutputOrigin(referenceImage->GetOrigin());
    resampleFilter->SetOutputDirection(referenceImage->GetDirection());

    using InterpolatorType =
        itk::LinearInterpolateImageFunction<ImageType, double>;
    InterpolatorType::Pointer interpolator = InterpolatorType::New();
    resampleFilter->SetInterpolator(interpolator);

    using TransformType = itk::AffineTransform<double, ImageType::ImageDimension>;
    TransformType::Pointer transform = TransformType::New();
    transform->SetIdentity();
    resampleFilter->SetTransform(transform);

    resampleFilter->Update();
    return resampleFilter->GetOutput();
}

ImageType::Pointer createImageFromVector(const std::vector<float>& imageData,
                                         ImageType::SizeType size) {
    ImageType::Pointer image = ImageType::New();
    ImageType::IndexType start;
    start.Fill(0);
    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(0);

    for (size_t x = 0; x < size[0]; ++x) {
        for (size_t y = 0; y < size[1]; ++y) {
            for (size_t z = 0; z < size[2]; ++z) {
                ImageType::IndexType index;
                index[0] = static_cast<ImageType::IndexValueType>(x);
                index[1] = static_cast<ImageType::IndexValueType>(y);
                index[2] = static_cast<ImageType::IndexValueType>(z);
                size_t vectorIndex = x * size[1] * size[2] + y * size[2] + z;
                image->SetPixel(index, imageData[vectorIndex]);
            }
        }
    }

    return image;
}

void extractImageData(ImageType::Pointer image, std::vector<float>& imageData) {
    ImageType::RegionType region = image->GetLargestPossibleRegion();
    ImageType::SizeType size = region.GetSize();

    imageData.resize(size[0] * size[1] * size[2]);
    for (size_t x = 0; x < size[0]; ++x) {
        for (size_t y = 0; y < size[1]; ++y) {
            for (size_t z = 0; z < size[2]; ++z) {
                ImageType::IndexType index;
                index[0] = static_cast<ImageType::IndexValueType>(x);
                index[1] = static_cast<ImageType::IndexValueType>(y);
                index[2] = static_cast<ImageType::IndexValueType>(z);
                float pixelValue = image->GetPixel(index);
                imageData[x * size[1] * size[2] + y * size[2] + z] = pixelValue;
            }
        }
    }
}

}  // namespace refactorCommon::image


