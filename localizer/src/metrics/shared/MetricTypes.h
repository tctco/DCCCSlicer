#pragma once

#include <iostream>
#include <map>
#include <string>

namespace Pipeline::Metrics {

struct MetricResult {
    std::string metricName;
    double suvr = 0.0;
    std::map<std::string, float> tracerValues;

    void printResult() const {
        std::cout << "Metric: " << metricName << std::endl;
        std::cout << "SUVr: " << suvr << std::endl;
        for (const auto& [tracer, value] : tracerValues) {
            std::cout << tracer << ": " << value << std::endl;
        }
        std::cout << std::endl;
    }
};

} // namespace Pipeline::Metrics
