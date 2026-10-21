#include "PetSegmenter.h"

#include <itkImageDuplicator.h>
#include <itkImageFileWriter.h>
#include <itkImageIOFactory.h>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "../common/NiftiIO.h"
#include "../common/PathUtils.h"
#include "../normalizers/RigidAlignmentNormalizer.h"

namespace Segmentation {
namespace {
size_t index(int x, int y, int z, const std::array<int, 3>& s) {
    return (size_t(z) * s[1] + y) * s[0] + x;
}
Eigen::Matrix4d geometry(ImageType::Pointer image) {
    Eigen::Matrix4d a = Eigen::Matrix4d::Identity();
    for (int i = 0; i < 3; ++i) {
        a(i, 3) = image->GetOrigin()[i];
        for (int j = 0; j < 3; ++j) a(i, j) = image->GetDirection()[i][j] * image->GetSpacing()[j];
    }
    if (!a.allFinite() || std::abs(a.determinant()) < 1e-10)
        throw std::runtime_error("Invalid spatial affine");
    return a;
}
nlohmann::json matrix(const Eigen::Matrix4d& a) {
    auto rows = nlohmann::json::array();
    for (int i = 0; i < 4; ++i) {
        auto row = nlohmann::json::array();
        for (int j = 0; j < 4; ++j) row.push_back(a(i, j));
        rows.push_back(row);
    }
    return rows;
}
ImageType::Pointer load3d(const std::string& path) { return Common::nifti::loadImage(path, true); }
}  // namespace
LabelImage::Pointer PetSegmenter::segment(const std::string& input, const Bundle& bundle,
                                          const Options& options) const {
    if (options.threads < 1) throw std::invalid_argument("threads must be positive");
    const auto lut = bundle.lookup(options.mode);
    auto native = load3d(input);
    const auto nativeGeometry = geometry(native);
    const auto region = native->GetLargestPossibleRegion();
    const auto n = region.GetNumberOfPixels();
    const auto* nativeData = native->GetBufferPointer();
    bool positive = false;
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(nativeData[i]))
            throw std::invalid_argument("PET must contain finite intensities");
        positive = positive || nativeData[i] > 0;
    }
    if (!positive) throw std::invalid_argument("PET must contain positive signal");
    ImageType::Pointer aligned;
    if (!options.rigidHeader.empty()) {
        aligned = load3d(options.rigidHeader);
        if (aligned->GetLargestPossibleRegion().GetSize() != region.GetSize())
            throw std::invalid_argument(
                "Rigid header must preserve native shape and voxel indexing");
        const auto* data = aligned->GetBufferPointer();
        for (size_t i = 0; i < n; ++i)
            if (!std::isfinite(data[i]) ||
                std::abs(data[i] - nativeData[i]) > 1e-6f + 1e-6f * std::abs(data[i]))
                throw std::invalid_argument(
                    "Rigid header values do not match native voxel indexing");
    } else {
        auto duplicate = itk::ImageDuplicator<ImageType>::New();
        duplicate->SetInputImage(native);
        duplicate->Update();
        aligned = duplicate->GetOutput();
        RigidAlignmentNormalizer rigid(config_);
        if (!options.debugPrefix.empty())
            rigid.setDebugMode(true, options.debugPrefix + "_alignment");
        // One initial prediction + one iterative prediction, threshold 0 disables early stopping.
        aligned = rigid.alignIterative(aligned, 1, 0.0f);
    }
    const auto rigidGeometry = geometry(aligned);
    Eigen::Matrix4d taskGeometry;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            taskGeometry(i, j) = bundle.affine[i][j] * (i < 2 ? -1 : 1);  // RAS -> LPS
    const auto taskToNative = (rigidGeometry.inverse() * taskGeometry).eval();
    auto task = ImageType::New();
    ImageType::SizeType taskSize;
    ImageType::SpacingType spacing;
    ImageType::PointType origin;
    ImageType::DirectionType direction;
    direction.SetIdentity();
    for (int i = 0; i < 3; ++i) {
        taskSize[i] = bundle.shape[i];
        spacing[i] = 1.5;
        origin[i] = taskGeometry(i, 3);
        for (int j = 0; j < 3; ++j) direction[i][j] = taskGeometry(i, j) / 1.5;
    }
    task->SetRegions(taskSize);
    task->SetSpacing(spacing);
    task->SetOrigin(origin);
    task->SetDirection(direction);
    task->Allocate();
    auto* taskData = task->GetBufferPointer();
    std::array<int, 3> nativeSize{};
    for (int d = 0; d < 3; ++d) nativeSize[d] = region.GetSize()[d];
    std::array<int, 3> start = bundle.shape, end{0, 0, 0};
    for (int z = 0; z < bundle.shape[2]; ++z)
        for (int y = 0; y < bundle.shape[1]; ++y)
            for (int x = 0; x < bundle.shape[0]; ++x) {
                const Eigen::Vector4d p = taskToNative * Eigen::Vector4d(x, y, z, 1);
                float value = 0;
                // scipy.ndimage affine_transform(order=1, mode='constant') boundary contract.
                if (p[0] >= 0 && p[1] >= 0 && p[2] >= 0 && p[0] <= nativeSize[0] - 1 &&
                    p[1] <= nativeSize[1] - 1 && p[2] <= nativeSize[2] - 1) {
                    const int lo[] = {int(std::floor(p[0])), int(std::floor(p[1])),
                                      int(std::floor(p[2]))};
                    const double f[] = {p[0] - lo[0], p[1] - lo[1], p[2] - lo[2]};
                    double total = 0;
                    for (int dz = 0; dz < 2; ++dz)
                        for (int dy = 0; dy < 2; ++dy)
                            for (int dx = 0; dx < 2; ++dx)
                                total += nativeData[index(std::min(lo[0] + dx, nativeSize[0] - 1),
                                                          std::min(lo[1] + dy, nativeSize[1] - 1),
                                                          std::min(lo[2] + dz, nativeSize[2] - 1),
                                                          nativeSize)] *
                                         (dx ? f[0] : 1 - f[0]) * (dy ? f[1] : 1 - f[1]) *
                                         (dz ? f[2] : 1 - f[2]);
                    value = static_cast<float>(total);
                }
                taskData[index(x, y, z, bundle.shape)] = value;
                if (value != 0) {
                    const int q[] = {x, y, z};
                    for (int d = 0; d < 3; ++d) {
                        start[d] = std::min(start[d], q[d]);
                        end[d] = std::max(end[d], q[d] + 1);
                    }
                }
            }
    std::array<int, 3> crop{};
    size_t count = 1;
    for (int d = 0; d < 3; ++d) {
        crop[d] = end[d] - start[d];
        if (crop[d] <= 0)
            throw std::runtime_error(
                "PET has no signal on the training grid after rigid alignment");
        count *= crop[d];
    }
    std::vector<float> data(count);
    double sum = 0;
    for (int z = 0; z < crop[2]; ++z)
        for (int y = 0; y < crop[1]; ++y)
            for (int x = 0; x < crop[0]; ++x) {
                const float v =
                    taskData[index(x + start[0], y + start[1], z + start[2], bundle.shape)];
                data[index(x, y, z, crop)] = v;
                sum += v;
            }
    const float mean = static_cast<float>(sum / count);
    double squared = 0;
    for (float v : data) {
        const float delta = v - mean;
        squared += static_cast<double>(delta) * delta;
    }
    const float deviation = std::max(static_cast<float>(std::sqrt(squared / count)), 1e-8f);
    for (auto& v : data) v = (v - mean) / deviation;
    if (!options.debugPrefix.empty()) {
        Common::nifti::saveImage(aligned, options.debugPrefix + "_rigid_header.nii.gz");
        Common::nifti::saveImage(task, options.debugPrefix + "_pet_task.nii.gz");
        auto normalized = ImageType::New();
        ImageType::SizeType s;
        for (int d = 0; d < 3; ++d) s[d] = crop[d];
        normalized->SetRegions(s);
        normalized->SetSpacing(spacing);
        normalized->SetDirection(direction);
        ImageType::IndexType begin;
        for (int d = 0; d < 3; ++d) begin[d] = start[d];
        ImageType::PointType o;
        task->TransformIndexToPhysicalPoint(begin, o);
        normalized->SetOrigin(o);
        normalized->Allocate();
        std::copy(data.begin(), data.end(), normalized->GetBufferPointer());
        Common::nifti::saveImage(normalized, options.debugPrefix + "_preprocessed.nii.gz");
    }
    const auto predicted = predict(data, crop, bundle, options);
    auto taskLabels = LabelImage::New();
    taskLabels->CopyInformation(task);
    taskLabels->SetRegions(taskSize);
    taskLabels->Allocate();
    taskLabels->FillBuffer(0);
    for (int z = 0; z < crop[2]; ++z)
        for (int y = 0; y < crop[1]; ++y)
            for (int x = 0; x < crop[0]; ++x)
                taskLabels->GetBufferPointer()[index(x + start[0], y + start[1], z + start[2],
                                                     bundle.shape)] =
                    predicted[index(x, y, z, crop)];
    auto result = LabelImage::New();
    result->CopyInformation(native);
    result->SetRegions(region);
    result->Allocate();
    result->FillBuffer(0);
    const auto nativeToTask = (taskGeometry.inverse() * rigidGeometry).eval();
    for (int z = 0; z < nativeSize[2]; ++z)
        for (int y = 0; y < nativeSize[1]; ++y)
            for (int x = 0; x < nativeSize[0]; ++x) {
                const Eigen::Vector4d p = nativeToTask * Eigen::Vector4d(x, y, z, 1);
                short id = 0;
                if (p[0] >= 0 && p[1] >= 0 && p[2] >= 0 && p[0] <= bundle.shape[0] - 1 &&
                    p[1] <= bundle.shape[1] - 1 && p[2] <= bundle.shape[2] - 1)
                    id = taskLabels->GetBufferPointer()[index(
                        int(std::floor(p[0] + 0.5)), int(std::floor(p[1] + 0.5)),
                        int(std::floor(p[2] + 0.5)), bundle.shape)];
                result->GetBufferPointer()[index(x, y, z, nativeSize)] = lut.at(id);
            }
    if (!options.debugPrefix.empty()) {
        saveLabels(taskLabels, options.debugPrefix + "_training_ids_task.nii.gz");
        Eigen::Matrix4d ras = Eigen::Matrix4d::Identity();
        ras(0, 0) = ras(1, 1) = -1;
        nlohmann::json receipt = {{"schema", "dccc_seg_native_cpp_v1"},
                                  {"native_affine", matrix(ras * nativeGeometry)},
                                  {"rigid_affine", matrix(ras * rigidGeometry)},
                                  {"crop_start_xyz", start},
                                  {"crop_shape_xyz", crop},
                                  {"normalization_mean", mean},
                                  {"normalization_std", deviation},
                                  {"tta", options.tta},
                                  {"tile_step_size", 0.5},
                                  {"threads", options.threads},
                                  {"mode", options.mode},
                                  {"provider", "CPUExecutionProvider"}};
        std::ofstream stream(Common::path::fromUtf8(options.debugPrefix + "_receipt.json"));
        if (!stream || !(stream << receipt.dump(2) << '\n'))
            throw std::runtime_error("Failed to write segmentation receipt");
    }
    return result;
}
void saveLabels(LabelImage::Pointer image, const std::string& path) {
    Common::nifti::saveLabelImage(image, path);
}
}  // namespace Segmentation
