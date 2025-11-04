#include "CenTauRzCalculator.h"
#include "SUVrCalculator.h"
#include "../utils/common.h"
#include <algorithm>
#include <string>

CenTauRzCalculator::CenTauRzCalculator(ConfigurationPtr config) : config_(config) {}

MetricResult CenTauRzCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
    std::string voiTemplatePath = config_->getMaskPath("centaur_voi");
    std::string refTemplatePath = config_->getMaskPath("centaur_ref");
    
    // Use SUVrCalculator utility method
    double suvr = SUVrCalculator::calculateSUVr(spatialNormalizedImage, voiTemplatePath, refTemplatePath);
    
    MetricResult result;
    result.metricName = "CenTauRz";
    result.suvr = suvr;
    
    // Calculate CenTauRz values using z-score formula
    auto tracerParams = getTracerParameters();
    for (const auto& [tracerName, params] : tracerParams) {
        float centaurz = suvr * params.slope + params.intercept;
        result.tracerValues[tracerName] = centaurz;
    }
    
    return result;
}

std::string CenTauRzCalculator::getName() const {
    return "CenTauRz";
}

std::vector<std::string> CenTauRzCalculator::getSupportedTracers() const {
    return {"FTP", "GTP1", "MK6240", "PI2620", "RO948", "PM-PBB3"};
}

std::map<std::string, CenTauRzCalculator::TracerParams> CenTauRzCalculator::getTracerParameters() const {
    std::map<std::string, TracerParams> params;
    
    // Read from configuration
    params["FTP"] = {
        config_->getFloat("centaurz.tracers.ftp.slope", 13.63f),
        config_->getFloat("centaurz.tracers.ftp.intercept", -15.85f)
    };
    params["GTP1"] = {
        config_->getFloat("centaurz.tracers.gtp1.slope", 10.67f),
        config_->getFloat("centaurz.tracers.gtp1.intercept", -11.92f)
    };
    params["MK6240"] = {
        config_->getFloat("centaurz.tracers.mk6240.slope", 10.08f),
        config_->getFloat("centaurz.tracers.mk6240.intercept", -10.06f)
    };
    params["PI2620"] = {
        config_->getFloat("centaurz.tracers.pi2620.slope", 8.45f),
        config_->getFloat("centaurz.tracers.pi2620.intercept", -9.61f)
    };
    params["RO948"] = {
        config_->getFloat("centaurz.tracers.ro948.slope", 13.05f),
        config_->getFloat("centaurz.tracers.ro948.intercept", -15.57f)
    };
    params["PM-PBB3"] = {
        config_->getFloat("centaurz.tracers.pmpbb3.slope", 16.73f),
        config_->getFloat("centaurz.tracers.pmpbb3.intercept", -15.34f)
    };
    
    return params;
}
