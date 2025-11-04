#pragma once
#include "../utils/common.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkIntensityWindowingImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkResampleImageFilter.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkConstantBoundaryCondition.h"
#include "itkLinearInterpolateImageFunction.h"
#include <vector>

/**
 * @brief Image preprocessing utilities for registration
 */
class ImagePreprocessor {
public:
    /**
     * @brief Preprocess image for rigid registration
     */
    static ImageType::Pointer preprocessForRigid(ImageType::Pointer image);
    
    /**
     * @brief Preprocess image for VoxelMorph registration
     */
    static ImageType::Pointer preprocessForVoxelMorph(ImageType::Pointer image);

private:
    // Image processing utilities
    static ImageType::Pointer clipIntensityPercentiles(ImageType::Pointer image, 
                                                       double lowerPercentile, 
                                                       double upperPercentile);
    static ImageType::Pointer gaussianSmooth(ImageType::Pointer image, double sigma);
    static ImageType::Pointer resampleImage(ImageType::Pointer image, 
                                           const ImageType::SpacingType& newSpacing);
    static ImageType::Pointer cropForeground(ImageType::Pointer image, float lowerThreshold);
    static ImageType::Pointer resizeImage(ImageType::Pointer image, 
                                         const ImageType::SizeType& newSize);
    
    // Helper functions
    static std::vector<double> getSortedPixelValues(ImageType::Pointer image);
    static double getPercentileValue(const std::vector<double>& sortedValues, double percentile);
};
