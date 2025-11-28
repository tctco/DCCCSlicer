#include "MetricCalculatorFactory.h"
#include "../calculators/CentiloidCalculator.h"
#include "../calculators/CenTauRCalculator.h"
#include "../calculators/CenTauRzCalculator.h"
#include "../calculators/SUVrCalculator.h"
#include "../calculators/FillStatesCalculator.h"
#include <stdexcept>
#include <algorithm>
#include <string>
#include <iostream>
#include <vector>

MetricCalculatorPtr MetricCalculatorFactory::create(CalculatorType type, ConfigurationPtr config) {
    switch (type) {
        case CalculatorType::CENTILOID:
            return std::make_shared<CentiloidCalculator>(config);
        case CalculatorType::CENTAUR:
            return std::make_shared<CenTauRCalculator>(config);
        case CalculatorType::CENTAURZ:
            return std::make_shared<CenTauRzCalculator>(config);
        case CalculatorType::SUVR:
            return std::make_shared<SUVrCalculator>(config);
        case CalculatorType::FILL_STATES:
            return std::make_shared<FillStatesCalculator>(config);
        default:
            throw std::invalid_argument("Unknown metric calculator type");
    }
}

MetricCalculatorPtr MetricCalculatorFactory::createFromString(const std::string& typeName, ConfigurationPtr config) {
    return create(stringToType(typeName), config);
}

std::vector<MetricCalculatorPtr> MetricCalculatorFactory::createAll(ConfigurationPtr config) {
    std::vector<MetricCalculatorPtr> calculators;
    calculators.push_back(create(CalculatorType::SUVR, config));
    calculators.push_back(create(CalculatorType::CENTILOID, config));
    calculators.push_back(create(CalculatorType::CENTAUR, config));
    calculators.push_back(create(CalculatorType::CENTAURZ, config));
    return calculators;
}

std::vector<MetricCalculatorPtr> MetricCalculatorFactory::createSelected(const std::string& selectedMetric, ConfigurationPtr config) {
    std::vector<MetricCalculatorPtr> calculators;
    
    // Create the selected calculator
    try {
        calculators.push_back(createFromString(selectedMetric, config));
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to create calculator for metric '" << selectedMetric << "': " << e.what() << std::endl;
    }
    
    return calculators;
}

std::vector<std::string> MetricCalculatorFactory::getAvailableTypes() {
    return {"suvr", "centiloid", "centaur", "centaurz", "fillstates"};
}

MetricCalculatorFactory::CalculatorType MetricCalculatorFactory::stringToType(const std::string& typeName) {
    std::string lowerName = typeName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName == "suvr") {
        return CalculatorType::SUVR;
    } else if (lowerName == "centiloid") {
        return CalculatorType::CENTILOID;
    } else if (lowerName == "centaur") {
        return CalculatorType::CENTAUR;
    } else if (lowerName == "centaurz") {
        return CalculatorType::CENTAURZ;
    } else if (lowerName == "fillstates") {
        return CalculatorType::FILL_STATES;
    }
    
    throw std::invalid_argument("Unsupported metric calculator type: " + typeName);
}

