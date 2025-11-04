#include "CenTauRCalculator.h"
#include "SUVrCalculator.h"
#include "../utils/common.h"
#include <algorithm>
#include <string>

CenTauRCalculator::CenTauRCalculator(ConfigurationPtr config) : config_(config) {}

MetricResult CenTauRCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
    std::string voiTemplatePath = config_->getMaskPath("centaur_voi");
    std::string refTemplatePath = config_->getMaskPath("centaur_ref");
    
    // Use SUVrCalculator utility method  
    double suvr = SUVrCalculator::calculateSUVr(spatialNormalizedImage, voiTemplatePath, refTemplatePath);
    
    MetricResult result;
    result.metricName = "CenTauR";
    result.suvr = suvr;
    
    // Calculate CenTauR values using percentile formula
    auto tracerParams = getTracerParameters();
    for (const auto& [tracerName, params] : tracerParams) {
        float centaur = (suvr - params.baselineSuvr) / (params.maxSuvr - params.baselineSuvr) * 100;
        result.tracerValues[tracerName] = centaur;
    }
    
    return result;
}

std::string CenTauRCalculator::getName() const {
    return "CenTauR";
}

std::vector<std::string> CenTauRCalculator::getSupportedTracers() const {
    return {"FTP", "GTP1", "MK6240", "PI2620", "RO948"};
}

std::map<std::string, CenTauRCalculator::TracerParams> CenTauRCalculator::getTracerParameters() const {
    std::map<std::string, TracerParams> params;
    
    // Read from configuration
    params["FTP"] = {
        config_->getFloat("centaur.tracers.ftp.baseline", 1.06f),
        config_->getFloat("centaur.tracers.ftp.max", 2.13f)
    };
    params["GTP1"] = {
        config_->getFloat("centaur.tracers.gtp1.baseline", 1.08f),
        config_->getFloat("centaur.tracers.gtp1.max", 1.69f)
    };
    params["MK6240"] = {
        config_->getFloat("centaur.tracers.mk6240.baseline", 0.93f),
        config_->getFloat("centaur.tracers.mk6240.max", 3.30f)
    };
    params["PI2620"] = {
        config_->getFloat("centaur.tracers.pi2620.baseline", 1.17f),
        config_->getFloat("centaur.tracers.pi2620.max", 2.12f)
    };
    params["RO948"] = {
        config_->getFloat("centaur.tracers.ro948.baseline", 1.03f),
        config_->getFloat("centaur.tracers.ro948.max", 2.40f)
    };
    
    return params;
}

