#include "ImageDefacer.h"

#include "../common/Common.h"
#include "../normalizers/RigidAlignmentNormalizer.h"
#include "../normalizers/VoxelMorphNormalizer.h"

#include <itkImageDuplicator.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionIteratorWithIndex.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Pipeline::Preprocessing {

namespace {

using BinaryImageType = Common::nifti::BinaryImageType;
constexpr double kInferiorDefacingTiltDegrees = 5.0;

ImageType::Pointer duplicateImage(ImageType::Pointer input) {
  using DuplicatorType = itk::ImageDuplicator<ImageType>;
  auto duplicator = DuplicatorType::New();
  duplicator->SetInputImage(input);
  duplicator->Update();
  auto output = duplicator->GetOutput();
  output->DisconnectPipeline();
  return output;
}

BinaryImageType::Pointer thresholdOnReference(ImageType::Pointer reference,
                                              ImageType::Pointer probability,
                                              float threshold) {
  auto resampled = Common::image::resampleToMatch(reference, probability);
  auto mask = BinaryImageType::New();
  mask->CopyInformation(reference);
  mask->SetRegions(reference->GetLargestPossibleRegion());
  mask->Allocate();

  itk::ImageRegionConstIterator<ImageType> inputIt(
      resampled, resampled->GetLargestPossibleRegion());
  itk::ImageRegionIterator<BinaryImageType> maskIt(
      mask, mask->GetLargestPossibleRegion());
  for (inputIt.GoToBegin(), maskIt.GoToBegin(); !inputIt.IsAtEnd();
       ++inputIt, ++maskIt) {
    maskIt.Set(inputIt.Get() >= threshold ? 1 : 0);
  }
  return mask;
}

itk::Vector<double, 3>
defacingPlaneNormalFromTemplate(ImageType::Pointer templateBrainMask) {
  itk::Vector<double, 3> anteriorAxis;
  itk::Vector<double, 3> superiorAxis;
  const auto direction = templateBrainMask->GetDirection();
  for (unsigned int dimension = 0; dimension < 3; ++dimension) {
    anteriorAxis[dimension] = direction[dimension][1];
    superiorAxis[dimension] = direction[dimension][2];
  }
  const double anteriorNorm = anteriorAxis.GetNorm();
  const double superiorNorm = superiorAxis.GetNorm();
  if (!(anteriorNorm > 0.0) || !(superiorNorm > 0.0)) {
    throw std::runtime_error("Template anatomical axes are invalid");
  }
  anteriorAxis /= anteriorNorm;
  superiorAxis /= superiorNorm;

  const double radians =
      kInferiorDefacingTiltDegrees * std::acos(-1.0) / 180.0;
  itk::Vector<double, 3> planeNormal;
  for (unsigned int dimension = 0; dimension < 3; ++dimension) {
    // Tilting the anterior normal toward inferior moves the cut posteriorly at
    // the nose and mouth while retaining a conservative frontal-lobe margin.
    planeNormal[dimension] = std::cos(radians) * anteriorAxis[dimension] -
                             std::sin(radians) * superiorAxis[dimension];
  }
  planeNormal /= planeNormal.GetNorm();
  return planeNormal;
}

double projection(const ImageType::PointType &point,
                  const itk::Vector<double, 3> &axis) {
  double value = 0.0;
  for (unsigned int dimension = 0; dimension < 3; ++dimension) {
    value += point[dimension] * axis[dimension];
  }
  return value;
}

template <typename TMaskImage>
double anteriorBrainBoundary(TMaskImage *brainMask,
                             const itk::Vector<double, 3> &anteriorAxis) {
  double boundary = -std::numeric_limits<double>::infinity();
  itk::ImageRegionConstIteratorWithIndex<TMaskImage> brainIt(
      brainMask, brainMask->GetLargestPossibleRegion());
  for (brainIt.GoToBegin(); !brainIt.IsAtEnd(); ++brainIt) {
    if (brainIt.Get() <= 0.5f) {
      continue;
    }
    typename TMaskImage::PointType point;
    brainMask->TransformIndexToPhysicalPoint(brainIt.GetIndex(), point);
    boundary = std::max(boundary, projection(point, anteriorAxis));
  }
  if (!std::isfinite(boundary)) {
    throw std::runtime_error("Brain mask is empty");
  }
  return boundary;
}

ImageType::Pointer createPlanarTemplateKeepMask(
    ImageType::Pointer templateBrainMask,
    const itk::Vector<double, 3> &anteriorAxis, float marginMm) {
  const auto region = templateBrainMask->GetLargestPossibleRegion();
  const double cut =
      anteriorBrainBoundary(templateBrainMask.GetPointer(), anteriorAxis) +
      marginMm;

  auto keepMask = ImageType::New();
  keepMask->CopyInformation(templateBrainMask);
  keepMask->SetRegions(region);
  keepMask->Allocate();

  itk::ImageRegionIteratorWithIndex<ImageType> keepIt(keepMask, region);
  for (keepIt.GoToBegin(); !keepIt.IsAtEnd(); ++keepIt) {
    ImageType::PointType point;
    keepMask->TransformIndexToPhysicalPoint(keepIt.GetIndex(), point);
    keepIt.Set(projection(point, anteriorAxis) <= cut ? 1.0f : 0.0f);
  }
  return keepMask;
}

BinaryImageType::Pointer createPlanarNativeKeepMask(
    ImageType::Pointer reference, BinaryImageType::Pointer nativeBrainMask,
    const itk::Vector<double, 3> &anteriorAxis, float marginMm) {
  const auto region = reference->GetLargestPossibleRegion();
  const double cut =
      anteriorBrainBoundary(nativeBrainMask.GetPointer(), anteriorAxis) +
      marginMm;

  auto keepMask = BinaryImageType::New();
  keepMask->CopyInformation(reference);
  keepMask->SetRegions(region);
  keepMask->Allocate();

  itk::ImageRegionIteratorWithIndex<BinaryImageType> keepIt(keepMask, region);
  for (keepIt.GoToBegin(); !keepIt.IsAtEnd(); ++keepIt) {
    ImageType::PointType point;
    reference->TransformIndexToPhysicalPoint(keepIt.GetIndex(), point);
    keepIt.Set(projection(point, anteriorAxis) <= cut ? 1 : 0);
  }
  return keepMask;
}

} // namespace

ImageDefacer::ImageDefacer(ConfigurationPtr config)
    : config_(std::move(config)) {
  if (!config_) {
    throw std::invalid_argument("ImageDefacer requires configuration");
  }
}

ImageType::Pointer
ImageDefacer::createTemplateKeepMask(ImageType::Pointer templateBrainMask,
                                     float marginMm) const {
  if (!templateBrainMask) {
    throw std::invalid_argument("Defacing requires a template brain mask");
  }
  if (marginMm < 0.0f) {
    throw std::invalid_argument("Defacing margin cannot be negative");
  }

  const auto planeNormal =
      defacingPlaneNormalFromTemplate(templateBrainMask);
  return createPlanarTemplateKeepMask(templateBrainMask, planeNormal,
                                      marginMm);
}

BinaryImageType::Pointer
ImageDefacer::createNativeKeepMask(ImageType::Pointer inputImage,
                                   ImageType::Pointer templateBrainMask,
                                   float threshold, bool useIterativeRigid,
                                   bool useManualFov, float marginMm) const {
  if (!inputImage || !templateBrainMask) {
    throw std::invalid_argument(
        "Native defacing requires an input image and template keep mask");
  }
  if (!(threshold > 0.0f && threshold < 1.0f)) {
    throw std::invalid_argument("Defacing threshold must be between 0 and 1");
  }
  if (marginMm < 0.0f) {
    throw std::invalid_argument("Defacing margin cannot be negative");
  }

  ImageType::Pointer rigidImage;
  if (useManualFov) {
    rigidImage = duplicateImage(inputImage);
  } else {
    RigidAlignmentNormalizer rigidNormalizer(config_);
    rigidImage = useIterativeRigid
                     ? rigidNormalizer.alignIterative(duplicateImage(inputImage))
                     : rigidNormalizer.align(duplicateImage(inputImage));
  }

  ImageType::Pointer inverseBrainMask;
  {
    VoxelMorphNormalizer voxelMorphNormalizer(config_,
                                              "affine_voxelmorph_inverse");
    inverseBrainMask =
        voxelMorphNormalizer.inverseWarp(rigidImage, templateBrainMask);
  }

  // Construct the cut after inverse-warping the brain mask. This preserves a
  // single physical plane on the native voxel lattice instead of bending the
  // defacing boundary with the nonlinear deformation field.
  auto nativeBrainMask =
      thresholdOnReference(rigidImage, inverseBrainMask, threshold);
  const auto planeNormal =
      defacingPlaneNormalFromTemplate(templateBrainMask);
  return createPlanarNativeKeepMask(rigidImage, nativeBrainMask, planeNormal,
                                    marginMm);
}

void ImageDefacer::applyMask(ImageType::Pointer image,
                             BinaryImageType::Pointer keepMask) {
  if (!image || !keepMask ||
      image->GetLargestPossibleRegion().GetSize() !=
          keepMask->GetLargestPossibleRegion().GetSize()) {
    throw std::invalid_argument("Defacing image and mask grids do not match");
  }

  itk::ImageRegionIterator<ImageType> imageIt(
      image, image->GetLargestPossibleRegion());
  itk::ImageRegionConstIterator<BinaryImageType> maskIt(
      keepMask, keepMask->GetLargestPossibleRegion());
  for (imageIt.GoToBegin(), maskIt.GoToBegin(); !imageIt.IsAtEnd();
       ++imageIt, ++maskIt) {
    if (maskIt.Get() == 0) {
      imageIt.Set(0.0f);
    }
  }
}

void ImageDefacer::applyMask(DynamicImageType::Pointer image,
                             BinaryImageType::Pointer keepMask) {
  if (!image || !keepMask) {
    throw std::invalid_argument(
        "Defacing dynamic image and mask cannot be null");
  }
  const auto imageSize = image->GetLargestPossibleRegion().GetSize();
  const auto maskSize = keepMask->GetLargestPossibleRegion().GetSize();
  if (imageSize[0] != maskSize[0] || imageSize[1] != maskSize[1] ||
      imageSize[2] != maskSize[2]) {
    throw std::invalid_argument(
        "Defacing dynamic image and mask grids do not match");
  }

  itk::ImageRegionIteratorWithIndex<DynamicImageType> imageIt(
      image, image->GetLargestPossibleRegion());
  const auto maskStart = keepMask->GetLargestPossibleRegion().GetIndex();
  for (imageIt.GoToBegin(); !imageIt.IsAtEnd(); ++imageIt) {
    const auto index = imageIt.GetIndex();
    BinaryImageType::IndexType maskIndex;
    maskIndex[0] = maskStart[0] + index[0] -
                   image->GetLargestPossibleRegion().GetIndex()[0];
    maskIndex[1] = maskStart[1] + index[1] -
                   image->GetLargestPossibleRegion().GetIndex()[1];
    maskIndex[2] = maskStart[2] + index[2] -
                   image->GetLargestPossibleRegion().GetIndex()[2];
    if (keepMask->GetPixel(maskIndex) == 0) {
      imageIt.Set(0.0f);
    }
  }
}

void ImageDefacer::applyTemplateMask(ImageType::Pointer image,
                                     ImageType::Pointer templateKeepMask,
                                     float threshold) {
  applyMask(image, thresholdOnReference(image, templateKeepMask, threshold));
}

} // namespace Pipeline::Preprocessing
