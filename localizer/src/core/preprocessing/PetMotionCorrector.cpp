#include "PetMotionCorrector.h"

#include "../common/NiftiIO.h"
#include "../common/PathUtils.h"
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <itkCenteredTransformInitializer.h>
#include <itkEuler3DTransform.h>
#include <itkExtractImageFilter.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegistrationMethodv4.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMattesMutualInformationImageToImageMetricv4.h>
#include <itkRegistrationParameterScalesFromPhysicalShift.h>
#include <itkRegularStepGradientDescentOptimizerv4.h>
#include <itkResampleImageFilter.h>
#include <stdexcept>
#include <zlib.h>

namespace Pipeline::Preprocessing {

namespace {

using TransformType = itk::Euler3DTransform<double>;

std::uint16_t byteSwap16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value >> 8U) | (value << 8U));
}

std::uint32_t byteSwap32(std::uint32_t value) {
    return ((value & 0x000000FFU) << 24U) |
           ((value & 0x0000FF00U) << 8U) |
           ((value & 0x00FF0000U) >> 8U) |
           ((value & 0xFF000000U) >> 24U);
}

std::uint64_t byteSwap64(std::uint64_t value) {
    return (static_cast<std::uint64_t>(byteSwap32(static_cast<std::uint32_t>(value))) << 32U) |
           byteSwap32(static_cast<std::uint32_t>(value >> 32U));
}

unsigned int declaredNiftiDimension(const std::string& inputPath) {
    const auto fileName = Common::path::legacyFileName(Common::path::fromUtf8(inputPath));
    gzFile input = gzopen(fileName.c_str(), "rb");
    if (!input) {
        throw std::runtime_error("Unable to open input image: " + inputPath);
    }
    std::array<unsigned char, 540> header{};
    const int bytesRead = gzread(input, header.data(), static_cast<unsigned int>(header.size()));
    gzclose(input);
    if (bytesRead < 48) {
        throw std::runtime_error("Input is too short to contain a valid NIfTI header: " + inputPath);
    }

    std::uint32_t headerSize = 0;
    std::memcpy(&headerSize, header.data(), sizeof(headerSize));
    bool swapBytes = false;
    if (headerSize != 348U && headerSize != 540U) {
        headerSize = byteSwap32(headerSize);
        swapBytes = true;
    }

    std::uint64_t dimension = 0;
    if (headerSize == 348U) {
        std::uint16_t nifti1Dimension = 0;
        std::memcpy(&nifti1Dimension, header.data() + 40, sizeof(nifti1Dimension));
        dimension = swapBytes ? byteSwap16(nifti1Dimension) : nifti1Dimension;
    } else if (headerSize == 540U) {
        std::memcpy(&dimension, header.data() + 16, sizeof(dimension));
        if (swapBytes) {
            dimension = byteSwap64(dimension);
        }
    } else {
        throw std::runtime_error("Input does not contain a valid NIfTI-1 or NIfTI-2 header: " +
                                 inputPath);
    }
    if (dimension < 1U || dimension > 7U) {
        throw std::runtime_error("Input NIfTI declares an invalid image dimension: " + inputPath);
    }
    return static_cast<unsigned int>(dimension);
}

ImageType::Pointer extractFrame(DynamicImageType::Pointer dynamicImage, unsigned int frame) {
    using ExtractFilterType = itk::ExtractImageFilter<DynamicImageType, ImageType>;
    auto region = dynamicImage->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto index = region.GetIndex();
    size[3] = 0;
    index[3] += static_cast<DynamicImageType::IndexValueType>(frame);

    auto extractor = ExtractFilterType::New();
    extractor->SetInput(dynamicImage);
    extractor->SetExtractionRegion({index, size});
    extractor->SetDirectionCollapseToSubmatrix();
    extractor->Update();
    return extractor->GetOutput();
}

TransformType::Pointer registerFrame(ImageType::Pointer fixed, ImageType::Pointer moving) {
    using MetricType = itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType>;
    using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;
    using RegistrationType = itk::ImageRegistrationMethodv4<ImageType, ImageType, TransformType>;
    using InitializerType = itk::CenteredTransformInitializer<TransformType, ImageType, ImageType>;
    using ScalesEstimatorType = itk::RegistrationParameterScalesFromPhysicalShift<MetricType>;

    auto transform = TransformType::New();
    auto initializer = InitializerType::New();
    initializer->SetTransform(transform);
    initializer->SetFixedImage(fixed);
    initializer->SetMovingImage(moving);
    initializer->MomentsOn();
    initializer->InitializeTransform();

    auto metric = MetricType::New();
    metric->SetNumberOfHistogramBins(50);

    auto optimizer = OptimizerType::New();
    optimizer->SetLearningRate(2.0);
    optimizer->SetMinimumStepLength(0.0005);
    optimizer->SetNumberOfIterations(250);
    optimizer->SetRelaxationFactor(0.5);
    optimizer->SetReturnBestParametersAndValue(true);

    auto scalesEstimator = ScalesEstimatorType::New();
    scalesEstimator->SetMetric(metric);
    scalesEstimator->SetTransformForward(true);
    optimizer->SetScalesEstimator(scalesEstimator);

    auto registration = RegistrationType::New();
    registration->SetFixedImage(fixed);
    registration->SetMovingImage(moving);
    registration->SetMetric(metric);
    registration->SetOptimizer(optimizer);
    registration->SetInitialTransform(transform);
    registration->InPlaceOn();
    registration->SetMetricSamplingStrategy(RegistrationType::MetricSamplingStrategyEnum::REGULAR);
    registration->SetMetricSamplingPercentage(0.25);

    RegistrationType::ShrinkFactorsArrayType shrinkFactors;
    shrinkFactors.SetSize(3);
    shrinkFactors[0] = 4;
    shrinkFactors[1] = 2;
    shrinkFactors[2] = 1;
    RegistrationType::SmoothingSigmasArrayType smoothingSigmas;
    smoothingSigmas.SetSize(3);
    smoothingSigmas[0] = 2;
    smoothingSigmas[1] = 1;
    smoothingSigmas[2] = 0;
    registration->SetNumberOfLevels(3);
    registration->SetShrinkFactorsPerLevel(shrinkFactors);
    registration->SetSmoothingSigmasPerLevel(smoothingSigmas);
    registration->SmoothingSigmasAreSpecifiedInPhysicalUnitsOn();
    registration->Update();
    return transform;
}

ImageType::Pointer resampleFrame(ImageType::Pointer moving,
                                 ImageType::Pointer fixed,
                                 TransformType::Pointer transform) {
    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
    auto resampler = ResampleFilterType::New();
    resampler->SetInput(moving);
    resampler->SetTransform(transform);
    resampler->SetInterpolator(InterpolatorType::New());
    resampler->SetUseReferenceImage(true);
    resampler->SetReferenceImage(fixed);
    resampler->SetDefaultPixelValue(0.0f);
    resampler->Update();
    return resampler->GetOutput();
}

DynamicImageType::Pointer allocateCorrectedDynamic(DynamicImageType::Pointer source) {
    auto output = DynamicImageType::New();
    output->SetRegions(source->GetLargestPossibleRegion());
    output->CopyInformation(source);
    output->Allocate();
    output->FillBuffer(0.0f);
    return output;
}

void copyFrameIntoDynamic(ImageType::Pointer frame,
                          DynamicImageType::Pointer dynamic,
                          unsigned int frameNumber) {
    itk::ImageRegionConstIterator<ImageType> sourceIt(frame, frame->GetLargestPossibleRegion());
    for (sourceIt.GoToBegin(); !sourceIt.IsAtEnd(); ++sourceIt) {
        const auto sourceIndex = sourceIt.GetIndex();
        DynamicImageType::IndexType targetIndex;
        targetIndex[0] = sourceIndex[0];
        targetIndex[1] = sourceIndex[1];
        targetIndex[2] = sourceIndex[2];
        targetIndex[3] = dynamic->GetLargestPossibleRegion().GetIndex()[3] +
                         static_cast<DynamicImageType::IndexValueType>(frameNumber);
        dynamic->SetPixel(targetIndex, sourceIt.Get());
    }
}

void addFrameToAverage(ImageType::Pointer frame, ImageType::Pointer average) {
    itk::ImageRegionConstIterator<ImageType> sourceIt(frame, frame->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageType> targetIt(average, average->GetLargestPossibleRegion());
    for (sourceIt.GoToBegin(), targetIt.GoToBegin(); !sourceIt.IsAtEnd(); ++sourceIt, ++targetIt) {
        targetIt.Set(targetIt.Get() + sourceIt.Get());
    }
}

} // namespace

unsigned int PetMotionCorrector::inspectImageDimension(const std::string& inputPath) {
    // ITK intentionally collapses trailing singleton axes. Read the declared NIfTI
    // dimension first so X×Y×Z×1 remains distinguishable from a true 3D image.
    return declaredNiftiDimension(inputPath);
}

PetMotionCorrectionResult PetMotionCorrector::correct(const std::string& inputPath,
                                                      bool retainCorrectedDynamic,
                                                      bool calculateAverage) const {
    const unsigned int dimension = inspectImageDimension(inputPath);
    if (dimension != 4) {
        if (dimension == 3) {
            throw std::invalid_argument(
                "pet-motion-correct requires a multi-frame/4D PET input; received a 3D image.");
        }
        throw std::invalid_argument(
            "pet-motion-correct requires a multi-frame/4D PET input; received a " +
            std::to_string(dimension) + "D image.");
    }

    using ReaderType = itk::ImageFileReader<DynamicImageType>;
    auto reader = ReaderType::New();
    reader->SetFileName(Common::path::legacyFileName(Common::path::fromUtf8(inputPath)));
    reader->Update();
    auto dynamicImage = reader->GetOutput();

    const auto numberOfFrames = dynamicImage->GetLargestPossibleRegion().GetSize()[3];
    if (numberOfFrames == 0) {
        throw std::invalid_argument("The 4D PET input contains no frames.");
    }

    PetMotionCorrectionResult result;
    if (retainCorrectedDynamic) {
        result.correctedDynamicImage = allocateCorrectedDynamic(dynamicImage);
    }
    auto fixed = extractFrame(dynamicImage, 0);
    if (calculateAverage) {
        result.averagedImage = ImageType::New();
        result.averagedImage->SetRegions(fixed->GetLargestPossibleRegion());
        result.averagedImage->CopyInformation(fixed);
        result.averagedImage->Allocate();
        result.averagedImage->FillBuffer(0.0f);
    }

    if (result.correctedDynamicImage) {
        copyFrameIntoDynamic(fixed, result.correctedDynamicImage, 0);
    }
    if (result.averagedImage) {
        addFrameToAverage(fixed, result.averagedImage);
    }
    result.motion.push_back({0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});

    for (unsigned int frame = 1; frame < numberOfFrames; ++frame) {
        auto moving = extractFrame(dynamicImage, frame);
        auto transform = registerFrame(fixed, moving);
        auto corrected = resampleFrame(moving, fixed, transform);
        if (result.correctedDynamicImage) {
            copyFrameIntoDynamic(corrected, result.correctedDynamicImage, frame);
        }
        if (result.averagedImage) {
            addFrameToAverage(corrected, result.averagedImage);
        }

        const auto parameters = transform->GetParameters();
        result.motion.push_back({frame,
                                 parameters[3], parameters[4], parameters[5],
                                 parameters[0], parameters[1], parameters[2]});
    }

    if (result.averagedImage) {
        const float scale = 1.0f / static_cast<float>(numberOfFrames);
        itk::ImageRegionIterator<ImageType> averageIt(
            result.averagedImage, result.averagedImage->GetLargestPossibleRegion());
        for (averageIt.GoToBegin(); !averageIt.IsAtEnd(); ++averageIt) {
            averageIt.Set(averageIt.Get() * scale);
        }
    }
    return result;
}

void PetMotionCorrector::saveAveragedImage(const PetMotionCorrectionResult& result,
                                           const std::string& outputPath) {
    if (!result.averagedImage) {
        throw std::invalid_argument(
            "An averaged PET image was not calculated for this motion-correction run.");
    }
    Common::nifti::saveImage(result.averagedImage, outputPath);
}

void PetMotionCorrector::saveCorrectedDynamicImage(const PetMotionCorrectionResult& result,
                                                   const std::string& outputPath) {
    if (!result.correctedDynamicImage) {
        throw std::invalid_argument(
            "Corrected dynamic frames were not retained for this motion-correction run.");
    }
    using WriterType = itk::ImageFileWriter<DynamicImageType>;
    auto writer = WriterType::New();
    writer->SetFileName(Common::path::legacyFileName(Common::path::fromUtf8(outputPath)));
    writer->SetInput(result.correctedDynamicImage);
    writer->Update();
}

void PetMotionCorrector::saveMotionParameters(const PetMotionCorrectionResult& result,
                                              const std::string& outputPath) {
    std::ofstream output(Common::path::fromUtf8(outputPath));
    if (!output) {
        throw std::runtime_error("Unable to open motion output: " + outputPath);
    }
    output << "frame\ttx\tty\ttz\trx\try\trz\n" << std::setprecision(12);
    for (const auto& motion : result.motion) {
        output << motion.frame << '\t'
               << motion.tx << '\t' << motion.ty << '\t' << motion.tz << '\t'
               << motion.rx << '\t' << motion.ry << '\t' << motion.rz << '\n';
    }
}

} // namespace Pipeline::Preprocessing
