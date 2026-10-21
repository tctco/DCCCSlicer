#include <onnxruntime_cxx_api.h>

#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "../common/OnnxPath.h"
#include "../common/PathUtils.h"
#include "PetSegmenter.h"

namespace Segmentation {
namespace {
size_t offset(int x, int y, int z, const std::array<int, 3>& s) {
    return (size_t(z) * s[1] + y) * s[0] + x;
}
float half(float value) { return static_cast<float>(Eigen::half(value)); }
}  // namespace
std::vector<short> predict(const std::vector<float>& normalized, const std::array<int, 3>& crop,
                           const Bundle& bundle, const Options& options) {
    // The validated 128x171x128 fixed grid always fits ONE 128x192x128 patch.
    // Thus official step=0.5 tiling has one position; no overlap accumulation.
    const std::array<int, 3> size{bundle.patch[2], bundle.patch[1], bundle.patch[0]};
    std::array<int, 3> pad{};
    for (int d = 0; d < 3; ++d) {
        if (crop[d] < 1 || crop[d] > size[d])
            throw std::runtime_error("Invalid preprocessed crop size");
        pad[d] = (size[d] - crop[d]) / 2;
    }
    const size_t count = size_t(size[0]) * size[1] * size[2];
    std::vector<float> padded(count, 0), input(count);
    for (int z = 0; z < crop[2]; ++z)
        for (int y = 0; y < crop[1]; ++y)
            for (int x = 0; x < crop[0]; ++x)
                padded[offset(x + pad[0], y + pad[1], z + pad[2], size)] =
                    normalized[offset(x, y, z, crop)];

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "DCCCseg");
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(options.threads);
    sessionOptions.SetInterOpNumThreads(1);
    const auto model =
        Common::onnx::makeOrtPath(Common::path::toUtf8(bundle.directory / "dccc_seg.onnx"));
    Ort::Session session(env, model.c_str(), sessionOptions);
    Ort::AllocatorWithDefaultOptions allocator;
    if (session.GetInputCount() != 1 || session.GetOutputCount() != 1 ||
        std::string(session.GetInputNameAllocated(0, allocator).get()) != "image" ||
        std::string(session.GetOutputNameAllocated(0, allocator).get()) != "logits")
        throw std::runtime_error("Segmentation ONNX tensor names/count mismatch");
    for (bool output : {false, true}) {
        auto info = output ? session.GetOutputTypeInfo(0) : session.GetInputTypeInfo(0);
        auto tensor = info.GetTensorTypeAndShapeInfo();
        auto shape = tensor.GetShape();
        if (tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 5 ||
            shape[0] != 1 || shape[1] != (output ? 96 : 1))
            throw std::runtime_error("Segmentation ONNX tensor type/shape mismatch");
        for (int d = 0; d < 3; ++d)
            if (shape[d + 2] > 0 && shape[d + 2] != bundle.patch[d])
                throw std::runtime_error("Segmentation ONNX spatial shape mismatch");
    }
    const std::array<int64_t, 5> shape{1, 1, size[2], size[1], size[0]};
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const char* inputNames[] = {"image"};
    const char* outputNames[] = {"logits"};
    std::vector<float> sum;
    // itertools.combinations order in nnU-Net: D,H,W,DH,DW,HW,DHW.
    const int masks[] = {0, 1, 2, 4, 3, 5, 6, 7};
    const int passes = options.tta ? 8 : 1;
    for (int pass = 0; pass < passes; ++pass) {
        const int mask = masks[pass];
        std::cout << "[seg] ONNX mirror " << pass + 1 << "/" << passes << std::endl;
        for (int z = 0; z < size[2]; ++z)
            for (int y = 0; y < size[1]; ++y)
                for (int x = 0; x < size[0]; ++x)
                    input[offset(x, y, z, size)] = padded[offset(
                        mask & 4 ? size[0] - 1 - x : x, mask & 2 ? size[1] - 1 - y : y,
                        mask & 1 ? size[2] - 1 - z : z, size)];
        auto value = Ort::Value::CreateTensor<float>(memory, input.data(), count, shape.data(),
                                                     shape.size());
        auto output = session.Run(Ort::RunOptions{nullptr}, inputNames, &value, 1, outputNames, 1);
        const std::vector<int64_t> expected{1, 96, size[2], size[1], size[0]};
        if (output[0].GetTensorTypeAndShapeInfo().GetShape() != expected)
            throw std::runtime_error("Unexpected segmentation logits shape");
        const auto* logits = output[0].GetTensorData<float>();
        if (pass == 0) sum.resize(count * 96, 0);
        for (int c = 0; c < 96; ++c)
            for (int z = 0; z < size[2]; ++z)
                for (int y = 0; y < size[1]; ++y)
                    for (int x = 0; x < size[0]; ++x) {
                        const auto i = offset(x, y, z, size);
                        const float v = logits[size_t(c) * count +
                                               offset(mask & 4 ? size[0] - 1 - x : x,
                                                      mask & 2 ? size[1] - 1 - y : y,
                                                      mask & 1 ? size[2] - 1 - z : z, size)];
                        if (!std::isfinite(v)) throw std::runtime_error("Nonfinite network logits");
                        sum[size_t(c) * count + i] += v;
                    }
    }
    // scipy gaussian_filter(delta) is separable; normalization cancels kernel
    // constants. Match its FP16 scaling and minimum nonzero replacement.
    std::vector<float> gaussian(count);
    float minimum = std::numeric_limits<float>::max();
    for (int z = 0; z < size[2]; ++z)
        for (int y = 0; y < size[1]; ++y)
            for (int x = 0; x < size[0]; ++x) {
                double exponent = 0;
                const int coords[] = {x, y, z};
                for (int d = 0; d < 3; ++d)
                    exponent += std::pow((coords[d] - size[d] / 2) / (size[d] / 8.0), 2);
                const float g = half(static_cast<float>(10 * std::exp(-0.5 * exponent)));
                gaussian[offset(x, y, z, size)] = g;
                if (g > 0) minimum = std::min(minimum, g);
            }
    for (auto& g : gaussian)
        if (g == 0) g = minimum;
    std::vector<short> result(normalized.size());
    for (int z = 0; z < crop[2]; ++z)
        for (int y = 0; y < crop[1]; ++y)
            for (int x = 0; x < crop[0]; ++x) {
                const auto i = offset(x + pad[0], y + pad[1], z + pad[2], size);
                float best = -std::numeric_limits<float>::infinity();
                short label = 0;
                for (short c = 0; c < 96; ++c) {
                    // torch: prediction (FP32) *= gaussian (FP16), += into FP16
                    // accumulator, then div(..., out=FP16). Preserve rounding and ties.
                    const float value = half(
                        half((sum[size_t(c) * count + i] / passes) * gaussian[i]) / gaussian[i]);
                    if (!std::isfinite(value))
                        throw std::runtime_error("Nonfinite accumulated logits");
                    if (value > best) {
                        best = value;
                        label = c;
                    }
                }
                result[offset(x, y, z, crop)] = label;
            }
    return result;
}
}  // namespace Segmentation
