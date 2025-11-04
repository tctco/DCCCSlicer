#include "ImagePreprocessor.h"
#include "itkAffineTransform.h"
#include <algorithm>

ImageType::Pointer ImagePreprocessor::preprocessForRigid(ImageType::Pointer image) {
    itk::Vector<double, 3> spacing;
    spacing.Fill(3.0);

    image = clipIntensityPercentiles(image, 0.01, 0.99);
    image = gaussianSmooth(image, 1);
    image = resampleImage(image, spacing);
    image = cropForeground(image, 0.35);

    itk::Size<3> outputSize;
    outputSize.Fill(64);
    image = resizeImage(image, outputSize);
    return image;
}

ImageType::Pointer ImagePreprocessor::preprocessForVoxelMorph(ImageType::Pointer image) {
    return clipIntensityPercentiles(image, 0.01, 0.99);
}

ImageType::Pointer ImagePreprocessor::clipIntensityPercentiles(ImageType::Pointer image,
                                                              double lowerPercentile,
                                                              double upperPercentile) {
    auto sortedVoxelValue = getSortedPixelValues(image);

    double lowerValue = getPercentileValue(sortedVoxelValue, lowerPercentile);
    double upperValue = getPercentileValue(sortedVoxelValue, upperPercentile);

    using IntensityWindowingImageFilterType =
        itk::IntensityWindowingImageFilter<ImageType, ImageType>;
    IntensityWindowingImageFilterType::Pointer windowingFilter =
        IntensityWindowingImageFilterType::New();
    windowingFilter->SetInput(image);
    windowingFilter->SetWindowMinimum(lowerValue);
    windowingFilter->SetWindowMaximum(upperValue);
    windowingFilter->SetOutputMinimum(lowerValue);
    windowingFilter->SetOutputMaximum(upperValue);
    windowingFilter->Update();

    using RescaleFilterType =
        itk::RescaleIntensityImageFilter<ImageType, ImageType>;
    auto rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(windowingFilter->GetOutput());
    rescaleFilter->SetOutputMinimum(0.0);
    rescaleFilter->SetOutputMaximum(1.0);
    rescaleFilter->Update();

    return rescaleFilter->GetOutput();
}

ImageType::Pointer ImagePreprocessor::gaussianSmooth(ImageType::Pointer image, double sigma) {
    using SmoothingFilterType =
        itk::DiscreteGaussianImageFilter<ImageType, ImageType>;
    using BoundaryConditionType = itk::ConstantBoundaryCondition<ImageType>;
    SmoothingFilterType::Pointer smoothingFilter = SmoothingFilterType::New();
    BoundaryConditionType boundaryCondition;
    boundaryCondition.SetConstant(-0.1);
    smoothingFilter->SetInputBoundaryCondition(&boundaryCondition);
    smoothingFilter->SetMaximumKernelWidth(9);
    smoothingFilter->SetInput(image);
    smoothingFilter->SetUseImageSpacingOff();
    smoothingFilter->SetVariance(sigma * sigma);
    smoothingFilter->Update();
    return smoothingFilter->GetOutput();
}

ImageType::Pointer ImagePreprocessor::resampleImage(ImageType::Pointer image,
                                                   const ImageType::SpacingType& newSpacing) {
    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;

    ResampleFilterType::Pointer resampler = ResampleFilterType::New();
    resampler->SetInput(image);
    resampler->SetOutputSpacing(newSpacing);
    ImageType::SizeType inputSize = image->GetLargestPossibleRegion().GetSize();
    ImageType::SpacingType inputSpacing = image->GetSpacing();

    ImageType::SizeType newSize;
    for (unsigned int i = 0; i < ImageType::ImageDimension; ++i) {
        newSize[i] = static_cast<unsigned int>(
            (inputSize[i] * inputSpacing[i]) / newSpacing[i] + 0.5);
    }

    resampler->SetSize(newSize);
    resampler->SetOutputOrigin(image->GetOrigin());
    resampler->SetOutputDirection(image->GetDirection());
    resampler->Update();

    return resampler->GetOutput();
}

ImageType::Pointer ImagePreprocessor::cropForeground(ImageType::Pointer image,
                                                    float lowerThreshold) {
    // Find bounding box
    ImageType::RegionType region = image->GetLargestPossibleRegion();
    ImageType::IndexType start = region.GetIndex();
    ImageType::SizeType size = region.GetSize();
    ImageType::IndexType minIndex = start;
    ImageType::IndexType maxIndex = start;
    bool initialized = false;

    itk::ImageRegionConstIterator<ImageType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > lowerThreshold) {
            ImageType::IndexType idx = it.GetIndex();
            if (!initialized) {
                minIndex = idx;
                maxIndex = idx;
                initialized = true;
            } else {
                for (unsigned int i = 0; i < 3; ++i) {
                    if (idx[i] < minIndex[i]) minIndex[i] = idx[i];
                    if (idx[i] > maxIndex[i]) maxIndex[i] = idx[i];
                }
            }
        }
    }

    if (!initialized) {
        std::cerr << "No foreground objects found!" << std::endl;
        return image;
    }

    // Calculate bounding box
    ImageType::IndexType startIndex = minIndex;
    ImageType::SizeType roiSize;
    for (unsigned int i = 0; i < 3; ++i) {
        roiSize[i] = maxIndex[i] - minIndex[i] + 1;
    }

    ImageType::RegionType desiredRegion;
    desiredRegion.SetIndex(startIndex);
    desiredRegion.SetSize(roiSize);

    // Crop the image using the bounding box
    using RegionOfInterestFilterType =
        itk::RegionOfInterestImageFilter<ImageType, ImageType>;
    RegionOfInterestFilterType::Pointer roiFilter =
        RegionOfInterestFilterType::New();
    roiFilter->SetInput(image);
    roiFilter->SetRegionOfInterest(desiredRegion);
    roiFilter->Update();

    return roiFilter->GetOutput();
}

ImageType::Pointer ImagePreprocessor::resizeImage(ImageType::Pointer image,
                                                 const ImageType::SizeType& newSize) {
    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    ResampleFilterType::Pointer resampler = ResampleFilterType::New();
    ImageType::SizeType originalSize =
        image->GetLargestPossibleRegion().GetSize();
    ImageType::SpacingType originalSpacing = image->GetSpacing();

    ImageType::SpacingType newSpacing;
    for (unsigned int i = 0; i < 3; ++i) {
        newSpacing[i] = originalSpacing[i] * static_cast<double>(originalSize[i]) /
                        static_cast<double>(newSize[i]);
    }

    resampler->SetInput(image);
    resampler->SetSize(newSize);
    resampler->SetOutputSpacing(newSpacing);
    resampler->SetOutputOrigin(image->GetOrigin());
    resampler->SetOutputDirection(image->GetDirection());
    resampler->SetTransform(itk::AffineTransform<double, 3>::New());
    resampler->SetInterpolator(
        itk::LinearInterpolateImageFunction<ImageType, double>::New());
    resampler->Update();

    return resampler->GetOutput();
}

std::vector<double> ImagePreprocessor::getSortedPixelValues(ImageType::Pointer image) {
    itk::ImageRegionIterator<ImageType> it(image, image->GetRequestedRegion());
    std::vector<double> pixelValues;

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        pixelValues.push_back(it.Get());
    }

    std::sort(pixelValues.begin(), pixelValues.end());
    return pixelValues;
}

double ImagePreprocessor::getPercentileValue(const std::vector<double>& sortedValues,
                                           double percentile) {
    size_t index = static_cast<size_t>(percentile * (sortedValues.size() - 1));
    return sortedValues[index];
}
