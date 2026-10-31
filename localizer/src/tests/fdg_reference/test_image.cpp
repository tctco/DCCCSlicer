#include "../../core/common/ImageOps.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Common::image;

int main() {
    ImageType::SizeType size;
    size[0] = 6; size[1] = 1; size[2] = 1;
    auto image = createImageFromVector({1000, 1, 9, 3, 7, 2000}, size);
    auto mask = createImageFromVector({0, 1, 1, 1, 1, 0}, size);
    const double divisor = calculateFdgTopHalfMeanInMask(image, mask);
    if (divisor != 8.0) throw std::runtime_error("Outside-mask values affected reference");
    divideVoxelsByValue(image, static_cast<float>(divisor));
    if (std::abs(calculateFdgTopHalfMeanInMask(image, mask) - 1.0) > 1e-6)
        throw std::runtime_error("Normalized reference top-half mean must equal one");
    std::vector<float> values;
    extractImageData(image, values);
    if (values[0] != 125 || values[5] != 250)
        throw std::runtime_error("Full image must be scaled by one divisor");
    mask->FillBuffer(0);
    bool rejected = false;
    try { calculateFdgTopHalfMeanInMask(image, mask); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Empty reference accepted");
    auto origin = mask->GetOrigin(); origin[0] += 2; mask->SetOrigin(origin);
    rejected = false;
    try { calculateFdgTopHalfMeanInMask(image, mask); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Mismatched reference grid accepted");
    auto legacy = createImageFromVector({0, 0, 1, 1, 2, 8}, size);
    normalizeFdgIterativeGlobalMean(legacy);
    extractImageData(legacy, values);
    const std::vector<float> expected{0, 0, 0.125f, 0.125f, 0.25f, 1};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (std::abs(values[i] - expected[i]) > 1e-6)
            throw std::runtime_error("Legacy FDG iterative normalization changed");
    }
    auto zero = createImageFromVector({0, 0, 0, 0, 0, 0}, size);
    rejected = false;
    try { normalizeFdgIterativeGlobalMean(zero); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Zero legacy FDG image accepted");
    std::cout << "FDG image normalization passed\n";
}
