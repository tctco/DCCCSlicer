#include "BrainExtractor.h"

#include "../normalizers/RigidAlignmentNormalizer.h"
#include "../normalizers/VoxelMorphNormalizer.h"

#include <itkContinuousIndex.h>
#include <itkImageDuplicator.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionIteratorWithIndex.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using BinaryImageType = Common::nifti::BinaryImageType;

BinaryImageType::Pointer createBinaryImage(
    ImageType::Pointer reference, const std::vector<unsigned char>& values) {
    auto output = BinaryImageType::New();
    output->CopyInformation(reference);
    output->SetRegions(reference->GetLargestPossibleRegion());
    output->Allocate();
    std::copy(values.begin(), values.end(), output->GetBufferPointer());
    return output;
}

BinaryImageType::Pointer createBinaryImage(
    BinaryImageType::Pointer reference, const std::vector<unsigned char>& values) {
    auto output = BinaryImageType::New();
    output->CopyInformation(reference);
    output->SetRegions(reference->GetLargestPossibleRegion());
    output->Allocate();
    std::copy(values.begin(), values.end(), output->GetBufferPointer());
    return output;
}

ImageType::Pointer duplicateImage(ImageType::Pointer input) {
    using DuplicatorType = itk::ImageDuplicator<ImageType>;
    auto duplicator = DuplicatorType::New();
    duplicator->SetInputImage(input);
    duplicator->Update();
    auto output = duplicator->GetOutput();
    output->DisconnectPipeline();
    return output;
}

ImageType::Pointer resampleToRigidGrid(ImageType::Pointer rigidImage,
                                       ImageType::Pointer inverseMask) {
    auto output = ImageType::New();
    output->CopyInformation(rigidImage);
    output->SetRegions(rigidImage->GetLargestPossibleRegion());
    output->Allocate();
    output->FillBuffer(0.0f);

    const auto inverseRegion = inverseMask->GetLargestPossibleRegion();
    itk::ImageRegionIteratorWithIndex<ImageType> outputIt(
        output, output->GetLargestPossibleRegion());
    for (outputIt.GoToBegin(); !outputIt.IsAtEnd(); ++outputIt) {
        ImageType::PointType point;
        output->TransformIndexToPhysicalPoint(outputIt.GetIndex(), point);
        itk::ContinuousIndex<double, ImageType::ImageDimension> continuousIndex;
        if (!inverseMask->TransformPhysicalPointToContinuousIndex(point, continuousIndex)) {
            continue;
        }

        ImageType::IndexType nearestIndex;
        for (unsigned int dimension = 0; dimension < ImageType::ImageDimension; ++dimension) {
            nearestIndex[dimension] = static_cast<ImageType::IndexValueType>(
                std::floor(continuousIndex[dimension] + 0.5));
        }
        if (inverseRegion.IsInside(nearestIndex)) {
            outputIt.Set(inverseMask->GetPixel(nearestIndex));
        }
    }
    return output;
}

BinaryImageType::Pointer largestComponent(BinaryImageType::Pointer input) {
    const auto region = input->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    const std::size_t sizeX = size[0];
    const std::size_t sizeY = size[1];
    const std::size_t sizeZ = size[2];
    const std::size_t strideY = sizeX;
    const std::size_t strideZ = sizeX * sizeY;
    const std::size_t voxelCount = strideZ * sizeZ;
    const unsigned char* values = input->GetBufferPointer();

    std::vector<unsigned char> visited(voxelCount, 0);
    std::vector<std::size_t> queue;
    std::vector<std::size_t> component;
    std::vector<std::size_t> largest;

    for (std::size_t seed = 0; seed < voxelCount; ++seed) {
        if (values[seed] == 0 || visited[seed] != 0) {
            continue;
        }
        queue.clear();
        component.clear();
        queue.push_back(seed);
        visited[seed] = 1;

        for (std::size_t head = 0; head < queue.size(); ++head) {
            const std::size_t index = queue[head];
            component.push_back(index);
            const std::size_t x = index % sizeX;
            const std::size_t y = (index / strideY) % sizeY;
            const std::size_t z = index / strideZ;

            const auto visit = [&](std::size_t neighbor) {
                if (values[neighbor] != 0 && visited[neighbor] == 0) {
                    visited[neighbor] = 1;
                    queue.push_back(neighbor);
                }
            };
            if (x > 0) visit(index - 1);
            if (x + 1 < sizeX) visit(index + 1);
            if (y > 0) visit(index - strideY);
            if (y + 1 < sizeY) visit(index + strideY);
            if (z > 0) visit(index - strideZ);
            if (z + 1 < sizeZ) visit(index + strideZ);
        }
        if (component.size() > largest.size()) {
            largest = component;
        }
    }

    auto output = BinaryImageType::New();
    output->CopyInformation(input);
    output->SetRegions(region);
    output->Allocate();
    output->FillBuffer(0);
    unsigned char* outputValues = output->GetBufferPointer();
    for (const std::size_t index : largest) {
        outputValues[index] = 1;
    }
    return output;
}

BinaryImageType::Pointer thresholdMask(ImageType::Pointer probability, float threshold) {
    const std::size_t voxelCount =
        probability->GetLargestPossibleRegion().GetNumberOfPixels();
    const float* input = probability->GetBufferPointer();
    std::vector<unsigned char> values(voxelCount, 0);
    for (std::size_t i = 0; i < voxelCount; ++i) {
        values[i] = input[i] >= threshold ? 1 : 0;
    }
    return createBinaryImage(probability, values);
}

BinaryImageType::Pointer closeMask(BinaryImageType::Pointer input) {
    const auto size = input->GetLargestPossibleRegion().GetSize();
    const std::size_t sizeX = size[0];
    const std::size_t sizeY = size[1];
    const std::size_t sizeZ = size[2];
    const std::size_t strideY = sizeX;
    const std::size_t strideZ = sizeX * sizeY;
    const std::size_t voxelCount = strideZ * sizeZ;
    const unsigned char* values = input->GetBufferPointer();
    std::vector<unsigned char> dilated(voxelCount, 0);
    std::vector<unsigned char> closed(voxelCount, 0);

    for (std::size_t index = 0; index < voxelCount; ++index) {
        const std::size_t x = index % sizeX;
        const std::size_t y = (index / strideY) % sizeY;
        const std::size_t z = index / strideZ;
        dilated[index] = values[index] != 0 ||
                         (x > 0 && values[index - 1] != 0) ||
                         (x + 1 < sizeX && values[index + 1] != 0) ||
                         (y > 0 && values[index - strideY] != 0) ||
                         (y + 1 < sizeY && values[index + strideY] != 0) ||
                         (z > 0 && values[index - strideZ] != 0) ||
                         (z + 1 < sizeZ && values[index + strideZ] != 0);
    }
    for (std::size_t index = 0; index < voxelCount; ++index) {
        const std::size_t x = index % sizeX;
        const std::size_t y = (index / strideY) % sizeY;
        const std::size_t z = index / strideZ;
        closed[index] = x > 0 && x + 1 < sizeX &&
                        y > 0 && y + 1 < sizeY &&
                        z > 0 && z + 1 < sizeZ &&
                        dilated[index] != 0 &&
                        dilated[index - 1] != 0 && dilated[index + 1] != 0 &&
                        dilated[index - strideY] != 0 &&
                        dilated[index + strideY] != 0 &&
                        dilated[index - strideZ] != 0 &&
                        dilated[index + strideZ] != 0;
    }
    return createBinaryImage(input, closed);
}

BinaryImageType::Pointer fillHoles(BinaryImageType::Pointer input) {
    const auto size = input->GetLargestPossibleRegion().GetSize();
    const std::size_t sizeX = size[0];
    const std::size_t sizeY = size[1];
    const std::size_t sizeZ = size[2];
    const std::size_t strideY = sizeX;
    const std::size_t strideZ = sizeX * sizeY;
    const std::size_t voxelCount = strideZ * sizeZ;
    const unsigned char* values = input->GetBufferPointer();
    std::vector<unsigned char> exterior(voxelCount, 0);
    std::vector<std::size_t> queue;

    const auto enqueue = [&](std::size_t index) {
        if (values[index] == 0 && exterior[index] == 0) {
            exterior[index] = 1;
            queue.push_back(index);
        }
    };
    for (std::size_t z = 0; z < sizeZ; ++z) {
        for (std::size_t y = 0; y < sizeY; ++y) {
            enqueue(y * strideY + z * strideZ);
            enqueue((sizeX - 1) + y * strideY + z * strideZ);
        }
    }
    for (std::size_t z = 0; z < sizeZ; ++z) {
        for (std::size_t x = 0; x < sizeX; ++x) {
            enqueue(x + z * strideZ);
            enqueue(x + (sizeY - 1) * strideY + z * strideZ);
        }
    }
    for (std::size_t y = 0; y < sizeY; ++y) {
        for (std::size_t x = 0; x < sizeX; ++x) {
            enqueue(x + y * strideY);
            enqueue(x + y * strideY + (sizeZ - 1) * strideZ);
        }
    }

    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::size_t index = queue[head];
        const std::size_t x = index % sizeX;
        const std::size_t y = (index / strideY) % sizeY;
        const std::size_t z = index / strideZ;
        if (x > 0) enqueue(index - 1);
        if (x + 1 < sizeX) enqueue(index + 1);
        if (y > 0) enqueue(index - strideY);
        if (y + 1 < sizeY) enqueue(index + strideY);
        if (z > 0) enqueue(index - strideZ);
        if (z + 1 < sizeZ) enqueue(index + strideZ);
    }

    std::vector<unsigned char> filled(voxelCount, 0);
    for (std::size_t i = 0; i < voxelCount; ++i) {
        filled[i] = values[i] != 0 || exterior[i] == 0 ? 1 : 0;
    }
    return createBinaryImage(input, filled);
}

BinaryImageType::Pointer cleanMask(ImageType::Pointer maskProbability, float threshold) {
    return largestComponent(fillHoles(closeMask(thresholdMask(maskProbability, threshold))));
}

std::size_t applyMask(ImageType::Pointer strippedImage,
                      BinaryImageType::Pointer brainMask) {
    itk::ImageRegionIterator<ImageType> imageIt(
        strippedImage, strippedImage->GetLargestPossibleRegion());
    itk::ImageRegionConstIterator<BinaryImageType> maskIt(
        brainMask, brainMask->GetLargestPossibleRegion());

    std::size_t voxelCount = 0;
    for (imageIt.GoToBegin(), maskIt.GoToBegin(); !imageIt.IsAtEnd();
         ++imageIt, ++maskIt) {
        if (maskIt.Get() == 0) {
            imageIt.Set(0.0f);
        } else {
            ++voxelCount;
        }
    }
    return voxelCount;
}

}  // namespace

BrainExtractor::BrainExtractor(ConfigurationPtr config) {
    if (!config) {
        throw std::invalid_argument("BrainExtractor requires configuration");
    }
    config_ = std::move(config);
}

BrainExtractor::Result BrainExtractor::extract(
    ImageType::Pointer inputImage, ImageType::Pointer templateBrainMask, float threshold) {
    if (!inputImage || !templateBrainMask) {
        throw std::invalid_argument(
            "Brain extraction requires an input image and template brain mask");
    }
    if (!(threshold > 0.0f && threshold < 1.0f)) {
        throw std::invalid_argument("Brain extraction threshold must be between 0 and 1");
    }

    ImageType::Pointer rigidImage;
    {
        // Keep the rigid and nonlinear ONNX sessions disjoint so their allocator
        // peaks cannot overlap on memory-constrained systems.
        RigidAlignmentNormalizer rigidNormalizer(config_);
        rigidNormalizer.setDebugMode(debugMode_, debugBasePath_);
        rigidImage = rigidNormalizer.align(duplicateImage(inputImage));
    }

    ImageType::Pointer inverseMask;
    {
        VoxelMorphNormalizer voxelMorphNormalizer(
            config_, "affine_voxelmorph_inverse");
        voxelMorphNormalizer.setDebugMode(debugMode_, debugBasePath_);
        inverseMask = voxelMorphNormalizer.inverseWarp(rigidImage, templateBrainMask);
    }
    ImageType::Pointer nativeProbability = resampleToRigidGrid(rigidImage, inverseMask);
    BinaryImageType::Pointer brainMask = cleanMask(nativeProbability, threshold);

    // The rigid operation only changes the physical header. The resampled mask now
    // has the native voxel lattice, so restore the source header for the saved mask.
    brainMask->SetOrigin(inputImage->GetOrigin());
    brainMask->SetSpacing(inputImage->GetSpacing());
    brainMask->SetDirection(inputImage->GetDirection());

    ImageType::Pointer strippedImage = duplicateImage(inputImage);
    const std::size_t brainVoxelCount = applyMask(strippedImage, brainMask);
    return Result{strippedImage, brainMask, brainVoxelCount};
}

void BrainExtractor::setDebugMode(bool enable, const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
}
