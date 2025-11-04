#pragma once
#include "../interfaces/IMetricCalculator.h"
#include "../interfaces/IConfiguration.h"
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

/**
 * @brief Metric calculator factory
 */
class MetricCalculatorFactory {
public:
    enum class CalculatorType {
        CENTILOID,
        CENTAUR,
        CENTAURZ,
        SUVR,
        // Future types can be added here
        // CUSTOM_METRIC
    };
    
    static MetricCalculatorPtr create(CalculatorType type, ConfigurationPtr config);
    static MetricCalculatorPtr createFromString(const std::string& typeName, ConfigurationPtr config);
    static std::vector<MetricCalculatorPtr> createAll(ConfigurationPtr config);
    static std::vector<MetricCalculatorPtr> createSelected(const std::string& selectedMetric, ConfigurationPtr config);
    static std::vector<std::string> getAvailableTypes();
    
private:
    static CalculatorType stringToType(const std::string& typeName);
};

