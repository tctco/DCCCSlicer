#pragma once
#include "../utils/common.h"
#include <memory>
#include <map>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Semi-quantitative metric calculation result
 */
struct MetricResult {
    std::string metricName;
    double suvr;
    std::map<std::string, float> tracerValues;  // tracer -> value mapping
    
    void printResult() const {
        std::cout << "Metric: " << metricName << std::endl;
        std::cout << "SUVr: " << suvr << std::endl;
        for (const auto& [tracer, value] : tracerValues) {
            std::cout << tracer << ": " << value << std::endl;
        }
        std::cout << std::endl;
    }
};

/**
 * @brief Semi-quantitative metric calculator interface
 * Defines common interface for all metric calculation algorithms
 */
class IMetricCalculator {
public:
    virtual ~IMetricCalculator() = default;
    
    /**
     * @brief Calculate semi-quantitative metrics
     * @param spatialNormalizedImage Spatially normalized image
     * @return Calculation result
     */
    virtual MetricResult calculate(ImageType::Pointer spatialNormalizedImage) = 0;
    
    /**
     * @brief Get calculator name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get list of supported tracers
     */
    virtual std::vector<std::string> getSupportedTracers() const = 0;
};

using MetricCalculatorPtr = std::shared_ptr<IMetricCalculator>;

