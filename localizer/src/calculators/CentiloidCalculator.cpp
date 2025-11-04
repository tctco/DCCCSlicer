#include "CentiloidCalculator.h"
#include "SUVrCalculator.h"
#include "../utils/common.h"
#include <algorithm>
#include <string>

CentiloidCalculator::CentiloidCalculator(ConfigurationPtr config) : config_(config) {}

MetricResult CentiloidCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
    std::string voiTemplatePath = config_->getMaskPath("centiloid_voi");
    std::string refTemplatePath = config_->getMaskPath("whole_cerebral");
    
    // Use SUVrCalculator utility method
    double suvr = SUVrCalculator::calculateSUVr(spatialNormalizedImage, voiTemplatePath, refTemplatePath);
    
    MetricResult result;
    result.metricName = "Centiloid";
    result.suvr = suvr;
    
    // Get tracer parameters and calculate Centiloid values for each tracer
    auto tracerParams = getTracerParameters();
    for (const auto& [tracerName, params] : tracerParams) {
        result.tracerValues[tracerName] = suvr * params.slope + params.intercept;
    }
    
    return result;
}

std::string CentiloidCalculator::getName() const {
    return "Centiloid";
}

std::vector<std::string> CentiloidCalculator::getSupportedTracers() const {
    return {"PiB", "FBP", "FBB", "FMM", "NAV"};
}

std::map<std::string, CentiloidCalculator::TracerParams> CentiloidCalculator::getTracerParameters() const {
    std::map<std::string, TracerParams> params;
    
    auto tracers = getSupportedTracers();
    for (const auto& tracer : tracers) {
        std::string tracerLower = tracer;
        std::transform(tracerLower.begin(), tracerLower.end(), tracerLower.begin(), ::tolower);
        
        TracerParams tracerParam;
        tracerParam.slope = config_->getFloat("centiloid.tracers." + tracerLower + ".slope");
        tracerParam.intercept = config_->getFloat("centiloid.tracers." + tracerLower + ".intercept");
        params[tracer] = tracerParam;
    }
    
    return params;
}

