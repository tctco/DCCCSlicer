#include "SegmentationBundle.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

#include "../common/PathUtils.h"

namespace Segmentation {
namespace {
nlohmann::json read(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("Missing segmentation bundle file: " + Common::path::toUtf8(path));
    return nlohmann::json::parse(stream);
}
void require(bool condition, const std::string& text) {
    if (!condition) throw std::runtime_error("Unsupported segmentation bundle: " + text);
}
}  // namespace
Bundle::Bundle(const std::filesystem::path& path) : directory(path) {
    const auto manifest = read(path / "manifest.json");
    const auto plans = read(path / "plans.json");
    const auto grid = read(path / "grid.json");
    const auto labels = read(path / "labels.json");
    const auto mapping = read(path / "neurosegmenter_mapping.json");
    const auto dataset = read(path / "dataset.json");
    require(manifest.at("schema") == "dccc_seg_onnx_v1", "manifest schema");
    require(
        manifest.at("input").at("name") == "image" && manifest.at("output").at("name") == "logits",
        "tensor names");
    require(manifest.at("input").at("dtype") == "float32" &&
                manifest.at("output").at("dtype") == "float32",
            "tensor dtype");
    require(manifest.at("output").at("channels") == 96 && labels.at("num_output_channels") == 96,
            "class count");
    require(manifest.at("mirroring_axes") == nlohmann::json::array({0, 1, 2}), "mirror axes");
    require(plans.at("transpose_forward") == nlohmann::json::array({0, 1, 2}) &&
                plans.at("transpose_backward") == nlohmann::json::array({0, 1, 2}),
            "axis order");
    require(plans.at("image_reader_writer") == "SimpleITKIO", "image reader");
    const auto& config = plans.at("configurations").at("3d_fullres");
    require(config.at("normalization_schemes") == nlohmann::json::array({"ZScoreNormalization"}) &&
                config.at("use_mask_for_norm") == nlohmann::json::array({false}),
            "normalization");
    require(config.at("spacing") == nlohmann::json::array({1.5, 1.5, 1.5}), "target spacing");
    require(dataset.at("labels").size() == 96 && dataset.at("labels") == labels.at("labels"),
            "dataset label mapping");
    std::set<int> trainingIds;
    for (const auto& value : labels.at("labels")) trainingIds.insert(value.get<int>());
    require(trainingIds.size() == 96 && *trainingIds.begin() == 0 && *trainingIds.rbegin() == 95,
            "dataset label mapping");
    shape = grid.at("shape").get<std::array<int, 3>>();
    affine = grid.at("affine").get<std::array<std::array<double, 4>, 4>>();
    patch = config.at("patch_size").get<std::array<int, 3>>();
    require(
        shape == std::array<int, 3>{128, 171, 128} && patch == std::array<int, 3>{128, 192, 128},
        "grid/patch shape");
    require(
        manifest.at("input").at("inference_shape") == nlohmann::json::array({1, 1, 128, 192, 128}),
        "inference shape");
    const std::array<std::array<double, 4>, 4> expected{
        {{-1.5, 0, 0, 94.25}, {0, 1.5, 0, -144.5}, {0, 0, 1.5, -86.25}, {0, 0, 0, 1}}};
    require(affine == expected, "training affine");
    rawIds = labels.at("raw_ids").get<std::vector<short>>();
    require(rawIds.size() == 96 && rawIds[0] == 0 &&
                std::set<short>(rawIds.begin(), rawIds.end()).size() == 96,
            "raw label mapping");
    neuroIds.resize(96, 0);
    nlohmann::json regionalMapping = nlohmann::json::object();
    std::set<int> regionIds;
    for (const auto& region : mapping.at("regions")) {
        const int id = region.at("id").get<int>();
        const bool available = region.at("available").get<bool>();
        require(id >= 1 && id <= 52 && regionIds.insert(id).second, "regional label mapping");
        require(available == (id != 45 && id != 46) && available == !region.at("raw_ids").empty(),
                "regional label mapping");
        for (const auto& raw : region.at("raw_ids")) {
            const int source = raw.get<int>();
            const std::string key = std::to_string(source);
            require(std::find(rawIds.begin(), rawIds.end(), source) != rawIds.end() &&
                        !regionalMapping.contains(key),
                    "regional label mapping");
            regionalMapping[key] = id;
        }
    }
    require(regionIds.size() == 52 && regionalMapping == mapping.at("raw_to_neuro"),
            "regional label mapping");
    for (size_t i = 0; i < 96; ++i) {
        require(rawIds[i] >= 0, "negative raw label");
        const auto key = std::to_string(rawIds[i]);
        require(labels.at("training_to_raw").at(std::to_string(i)) == rawIds[i] &&
                    labels.at("raw_to_training").at(key) == i,
                "nonreversible labels");
        if (mapping.at("raw_to_neuro").contains(key))
            neuroIds[i] = mapping.at("raw_to_neuro").at(key).get<short>();
        require(neuroIds[i] >= 0 && neuroIds[i] <= 52 && neuroIds[i] != 45 && neuroIds[i] != 46,
                "unavailable brainstem/pons labels");
        require(rawIds[i] != 16 || neuroIds[i] == 0, "whole brainstem must not be split");
    }
    require(std::filesystem::is_regular_file(path / "dccc_seg.onnx"), "missing dccc_seg.onnx");
}
std::vector<short> Bundle::lookup(const std::string& mode) const {
    if (mode == "freesurfer") return rawIds;
    if (mode == "neurosegmenter") return neuroIds;
    if (mode == "training") {
        std::vector<short> result(96);
        for (short i = 0; i < 96; ++i) result[i] = i;
        return result;
    }
    throw std::invalid_argument("Unknown segmentation mode: " + mode);
}
}  // namespace Segmentation
