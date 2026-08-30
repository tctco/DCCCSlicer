#include "ImageOps.h"

#include <itkImageFileWriter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkLabelStatisticsImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkWarpImageFilter.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace Common::image {

void divideVoxelsByValue(ImageType::Pointer image, float divisor) {
    itk::ImageRegionIterator<ImageType> it(
        image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        it.Set(it.Get() / divisor);
    }
}

void normalizeFdgIterativeGlobalMean(ImageType::Pointer image) {
    if (!image) {
        throw std::invalid_argument("FDG global mean normalization requires an image");
    }

    const auto region = image->GetLargestPossibleRegion();
    const auto voxelCount = region.GetNumberOfPixels();
    if (voxelCount == 0) {
        throw std::invalid_argument("FDG global mean normalization requires a non-empty image");
    }

    double globalSum = 0.0;
    itk::ImageRegionConstIterator<ImageType> inputIt(image, region);
    for (inputIt.GoToBegin(); !inputIt.IsAtEnd(); ++inputIt) {
        globalSum += static_cast<double>(inputIt.Get());
    }
    const double globalMean = globalSum / static_cast<double>(voxelCount);
    if (!std::isfinite(globalMean) || globalMean <= 0.0) {
        throw std::runtime_error("Invalid whole-image mean for FDG normalization");
    }
    divideVoxelsByValue(image, static_cast<float>(globalMean));

    std::size_t previousMaskedCount = std::numeric_limits<std::size_t>::max();
    while (true) {
        std::size_t retainedCount = 0;
        double retainedSum = 0.0;
        itk::ImageRegionConstIterator<ImageType> it(image, region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            const double value = static_cast<double>(it.Get());
            if (value >= 0.5) {
                retainedSum += value;
                ++retainedCount;
            }
        }

        const std::size_t maskedCount = voxelCount - retainedCount;
        if (maskedCount == previousMaskedCount) {
            break;
        }
        if (retainedCount == 0) {
            throw std::runtime_error("FDG normalization mask contains no voxels at or above 0.5");
        }

        const double retainedMean = retainedSum / static_cast<double>(retainedCount);
        if (!std::isfinite(retainedMean) || retainedMean <= 0.0) {
            throw std::runtime_error("Invalid masked mean for FDG normalization");
        }
        divideVoxelsByValue(image, static_cast<float>(retainedMean));
        previousMaskedCount = maskedCount;
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

}  // namespace Common::image
