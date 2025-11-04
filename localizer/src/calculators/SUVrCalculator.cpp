#include "SUVrCalculator.h"
#include "../utils/common.h"
#include <algorithm>
#include <string>

SUVrCalculator::SUVrCalculator(ConfigurationPtr config) : config_(config) {}

MetricResult SUVrCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
    MetricResult result;
    result.metricName = "SUVr";
    
    // Get region configurations
    auto regionConfigs = getRegionConfigurations();
    
    if (regionConfigs.empty()) {
        // Default to centiloid regions if no specific configuration
        std::string voiPath = config_->getMaskPath("centiloid_voi");
        std::string refPath = config_->getMaskPath("whole_cerebral");
        result.suvr = calculateSUVr(spatialNormalizedImage, voiPath, refPath);
        result.tracerValues["Default"] = static_cast<float>(result.suvr);
    } else {
        // Calculate SUVr for each configured region pair
        double primarySUVr = 0.0;
        bool primarySet = false;
        
        for (const auto& regionConfig : regionConfigs) {
            std::string voiPath = config_->getMaskPath(regionConfig.voiMaskKey);
            std::string refPath = config_->getMaskPath(regionConfig.refMaskKey);
            
            double suvr = calculateSUVr(spatialNormalizedImage, voiPath, refPath);
            result.tracerValues[regionConfig.description] = static_cast<float>(suvr);
            
            // Use first configuration as primary SUVr
            if (!primarySet) {
                result.suvr = suvr;
                primarySet = true;
            }
        }
    }
    
    return result;
}

std::string SUVrCalculator::getName() const {
    return "SUVr";
}

std::vector<std::string> SUVrCalculator::getSupportedTracers() const {
    auto regionConfigs = getRegionConfigurations();
    std::vector<std::string> tracers;
    
    if (regionConfigs.empty()) {
        tracers.push_back("Default");
    } else {
        for (const auto& config : regionConfigs) {
            tracers.push_back(config.description);
        }
    }
    
    return tracers;
}

double SUVrCalculator::calculateSUVr(ImageType::Pointer spatialNormalizedImage, 
                                     const std::string& voiMaskPath, 
                                     const std::string& refMaskPath) {
    ImageType::Pointer voiTemplate = Common::LoadNii(voiMaskPath);
    ImageType::Pointer refTemplate = Common::LoadNii(refMaskPath);
    ImageType::Pointer resampledImage = Common::ResampleToMatch(voiTemplate, spatialNormalizedImage);
    
    double meanVoi = Common::CalculateMeanInMask(resampledImage, voiTemplate);
    double meanRef = Common::CalculateMeanInMask(resampledImage, refTemplate);
    
    return meanVoi / meanRef;
}

std::vector<SUVrCalculator::SUVrRegionConfig> SUVrCalculator::getRegionConfigurations() const {
    std::vector<SUVrRegionConfig> configs;
    
    // Read from configuration file
    // For now, provide some default configurations
    // This can be extended to read from config.ini
    
    // Centiloid regions
    if (config_->getMaskPath("centiloid_voi") != "" && config_->getMaskPath("whole_cerebral") != "") {
        configs.push_back({"centiloid_voi", "whole_cerebral", "Centiloid_SUVr"});
    }
    
    // CenTauR regions  
    if (config_->getMaskPath("centaur_voi") != "" && config_->getMaskPath("centaur_ref") != "") {
        configs.push_back({"centaur_voi", "centaur_ref", "CenTauR_SUVr"});
    }
    
    return configs;
}
