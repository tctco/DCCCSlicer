#pragma once
#include <array>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Segmentation {
struct Bundle {
    std::filesystem::path directory;
    std::array<int, 3> shape{}, patch{};            // X,Y,Z and D,H,W respectively
    std::array<std::array<double, 4>, 4> affine{};  // NIfTI RAS
    std::vector<short> rawIds, neuroIds;
    explicit Bundle(const std::filesystem::path& path);
    std::vector<short> lookup(const std::string& mode) const;
};
}  // namespace Segmentation
