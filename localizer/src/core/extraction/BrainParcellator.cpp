#include "BrainParcellator.h"

#include "../normalizers/RigidAlignmentNormalizer.h"
#include "../normalizers/VoxelMorphNormalizer.h"

#include <itkContinuousIndex.h>
#include <itkImageDuplicator.h>
#include <itkImageRegionIteratorWithIndex.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using BinaryImageType = Common::nifti::BinaryImageType;

struct TemplateRegionDefinition {
    const char* key;
    std::vector<int> labelIds;
};

const std::vector<TemplateRegionDefinition>& regionalDefinitions() {
    static const std::vector<TemplateRegionDefinition> definitions = {
        {"ventricles", {4, 5, 43, 44}},
        {"left_hippocampus", {17}},
        {"right_hippocampus", {53}},
        {"left_entorhinal", {1006}},
        {"right_entorhinal", {2006}},
        {"left_fusiform", {1007}},
        {"right_fusiform", {2007}},
        {"left_middle_temporal", {1015}},
        {"right_middle_temporal", {2015}},
    };
    return definitions;
}

bool containsLabel(const std::vector<int>& labels, int value) {
    return std::find(labels.begin(), labels.end(), value) != labels.end();
}

bool isWholeBrainLabel(int label) {
    if ((label >= 1000 && label <= 1035) ||
        (label >= 2000 && label <= 2035) ||
        (label >= 251 && label <= 255)) {
        return true;
    }
    static constexpr std::array<int, 25> brainLabels = {
        2, 7, 8, 10, 11, 12, 13, 16, 17, 18, 26, 28, 41,
        46, 47, 49, 50, 51, 52, 53, 54, 58, 60, 77, 85};
    return std::find(brainLabels.begin(), brainLabels.end(), label) !=
           brainLabels.end();
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

ImageType::Pointer makeTemplateMask(
    ImageType::Pointer atlas, const std::function<bool(int)>& includeLabel) {
    auto mask = ImageType::New();
    mask->CopyInformation(atlas);
    mask->SetRegions(atlas->GetLargestPossibleRegion());
    mask->Allocate();

    const std::size_t voxelCount =
        atlas->GetLargestPossibleRegion().GetNumberOfPixels();
    const float* atlasValues = atlas->GetBufferPointer();
    float* maskValues = mask->GetBufferPointer();
    for (std::size_t i = 0; i < voxelCount; ++i) {
        const int label = static_cast<int>(std::lround(atlasValues[i]));
        maskValues[i] = includeLabel(label) ? 1.0f : 0.0f;
    }
    return mask;
}

ImageType::Pointer resampleToRigidGrid(ImageType::Pointer rigidImage,
                                       ImageType::Pointer inverseProbability) {
    auto output = ImageType::New();
    output->CopyInformation(rigidImage);
    output->SetRegions(rigidImage->GetLargestPossibleRegion());
    output->Allocate();
    output->FillBuffer(0.0f);

    const auto inverseRegion = inverseProbability->GetLargestPossibleRegion();
    itk::ImageRegionIteratorWithIndex<ImageType> outputIt(
        output, output->GetLargestPossibleRegion());
    for (outputIt.GoToBegin(); !outputIt.IsAtEnd(); ++outputIt) {
        ImageType::PointType point;
        output->TransformIndexToPhysicalPoint(outputIt.GetIndex(), point);
        itk::ContinuousIndex<double, ImageType::ImageDimension> continuousIndex;
        if (!inverseProbability->TransformPhysicalPointToContinuousIndex(
                point, continuousIndex)) {
            continue;
        }

        ImageType::IndexType nearestIndex;
        for (unsigned int dimension = 0;
             dimension < ImageType::ImageDimension; ++dimension) {
            nearestIndex[dimension] = static_cast<ImageType::IndexValueType>(
                std::floor(continuousIndex[dimension] + 0.5));
        }
        if (inverseRegion.IsInside(nearestIndex)) {
            outputIt.Set(inverseProbability->GetPixel(nearestIndex));
        }
    }
    return output;
}

BinaryImageType::Pointer thresholdMask(ImageType::Pointer probability,
                                       ImageType::Pointer nativeReference,
                                       float threshold,
                                       std::size_t& voxelCount) {
    auto mask = BinaryImageType::New();
    mask->CopyInformation(nativeReference);
    mask->SetRegions(nativeReference->GetLargestPossibleRegion());
    mask->Allocate();

    const std::size_t count =
        probability->GetLargestPossibleRegion().GetNumberOfPixels();
    const float* input = probability->GetBufferPointer();
    unsigned char* output = mask->GetBufferPointer();
    voxelCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        output[i] = input[i] >= threshold ? 1 : 0;
        voxelCount += output[i] != 0 ? 1 : 0;
    }
    return mask;
}

double voxelVolumeMl(ImageType::Pointer image) {
    const auto spacing = image->GetSpacing();
    return static_cast<double>(spacing[0]) *
           static_cast<double>(spacing[1]) *
           static_cast<double>(spacing[2]) / 1000.0;
}

}  // namespace

BrainParcellator::BrainParcellator(ConfigurationPtr config) {
    if (!config) {
        throw std::invalid_argument("BrainParcellator requires configuration");
    }
    config_ = std::move(config);
}

BrainParcellator::Result BrainParcellator::parcellate(
    ImageType::Pointer inputImage,
    ImageType::Pointer templateAparcAseg,
    ImageType::Pointer templateIntracranialMask,
    float threshold) {
    if (!inputImage || !templateAparcAseg || !templateIntracranialMask) {
        throw std::invalid_argument(
            "Brain parcellation requires input, aparc+aseg, and intracranial images");
    }
    if (!(threshold > 0.0f && threshold < 1.0f)) {
        throw std::invalid_argument("Brain parcellation threshold must be between 0 and 1");
    }

    ImageType::Pointer rigidImage;
    {
        RigidAlignmentNormalizer rigidNormalizer(config_);
        rigidNormalizer.setDebugMode(debugMode_, debugBasePath_);
        rigidImage = rigidNormalizer.align(duplicateImage(inputImage));
    }

    std::vector<std::string> regionKeys;
    std::vector<ImageType::Pointer> templateMasks;
    for (const auto& definition : regionalDefinitions()) {
        regionKeys.emplace_back(definition.key);
        templateMasks.push_back(makeTemplateMask(
            templateAparcAseg,
            [&definition](int label) {
                return containsLabel(definition.labelIds, label);
            }));
    }
    regionKeys.emplace_back("whole_brain");
    templateMasks.push_back(makeTemplateMask(templateAparcAseg, isWholeBrainLabel));
    regionKeys.emplace_back("intracranial");
    templateMasks.push_back(templateIntracranialMask);

    std::vector<ImageType::Pointer> inverseProbabilities;
    {
        VoxelMorphNormalizer voxelMorphNormalizer(
            config_, "affine_voxelmorph_inverse");
        voxelMorphNormalizer.setDebugMode(debugMode_, debugBasePath_);
        inverseProbabilities = voxelMorphNormalizer.inverseWarpChannels(
            rigidImage, templateMasks);
    }
    templateMasks.clear();

    Result result;
    result.voxelVolumeMl = voxelVolumeMl(inputImage);
    result.regions.reserve(regionKeys.size());
    for (std::size_t i = 0; i < regionKeys.size(); ++i) {
        ImageType::Pointer nativeProbability =
            resampleToRigidGrid(rigidImage, inverseProbabilities[i]);
        std::size_t count = 0;
        BinaryImageType::Pointer mask = thresholdMask(
            nativeProbability, inputImage, threshold, count);
        result.regions.push_back(
            Region{regionKeys[i], mask, count, count * result.voxelVolumeMl});
    }
    return result;
}

void BrainParcellator::setDebugMode(bool enable, const std::string& basePath) {
    debugMode_ = enable;
    debugBasePath_ = basePath;
}
