#include "../core/common/FdgReferenceStatistics.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using Common::image::fdgTopHalfMean;
int main() {
    const auto check = [](double actual, double expected) {
        if (std::abs(actual - expected) > 1e-10)
            throw std::runtime_error("Incorrect FDG top-half mean");
    };
    check(fdgTopHalfMean({1, 9, 3, 7}), 8);
    check(fdgTopHalfMean({1, 9, 3, 7, 5}), 7);
    check(fdgTopHalfMean({0, 0, 4, 4}), 4);
    check(fdgTopHalfMean({2}), 2);
    check(fdgTopHalfMean({2, 2, 2}), 2);
    // Scaling all reference samples must scale the divisor identically.
    check(fdgTopHalfMean({10, 90, 30, 70}), 80);
    for (const auto& values : std::vector<std::vector<float>>{
             {}, {0, 0}, {-2, -1}, {1, std::numeric_limits<float>::quiet_NaN()},
             {1, std::numeric_limits<float>::infinity()}}) {
        bool rejected = false;
        try { fdgTopHalfMean(values); }
        catch (const std::exception&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid FDG reference accepted");
    }
    std::cout << "FDG reference statistics passed\n";
}
