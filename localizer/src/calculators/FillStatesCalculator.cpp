#include "FillStatesCalculator.h"

#include <filesystem>
#include <stdexcept>
#include <cctype>

#include <itkImageRegionIterator.h>

FillStatesCalculator::FillStatesCalculator(ConfigurationPtr config)
    : config_(std::move(config)) {}

std::string FillStatesCalculator::toLowerCopy(const std::string& v) {
    std::string lower = v;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

void FillStatesCalculator::setTracer(const std::string& tracer) {
    tracer_ = toLowerCopy(tracer);
}

ImageType::Pointer FillStatesCalculator::getLastMaskImage() const {
    return lastMaskImage_;
}

FillStatesCalculator::TracerResources FillStatesCalculator::getTracerResources() const {
    TracerResources res;

    const std::string t = toLowerCopy(tracer_);
    const std::string baseKey = "fillstates.tracers." + t;

    const std::string meanRel = config_->getString(baseKey + ".mean");
    const std::string stdRel  = config_->getString(baseKey + ".std");
    const std::string roiRel  = config_->getString(baseKey + ".roi");

    if (meanRel.empty() || stdRel.empty() || roiRel.empty()) {
        throw std::runtime_error(
            "Missing fillstates configuration for tracer '" + t +
            "'. Please set fillstates.tracers." + t + ".mean/std/roi in config.");
    }

    const std::string execDir = Common::getExecutablePath();
    res.meanPath = (std::filesystem::path(execDir) / meanRel).string();
    res.stdPath  = (std::filesystem::path(execDir) / stdRel).string();
    res.roiPath  = (std::filesystem::path(execDir) / roiRel).string();

    if (t == "fbp" || t == "fdg") {
        res.refMaskKey = "whole_cerebral"; // Centiloid-style reference
    } else if (t == "ftp") {
        res.refMaskKey = "centaur_ref";    // CenTauRz-style reference
    } else {
        throw std::invalid_argument("Unsupported tracer for fill-states: " + tracer_);
    }

    return res;
}

MetricResult FillStatesCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
    if (!spatialNormalizedImage) {
        throw std::invalid_argument("FillStatesCalculator::calculate received null image.");
    }

    if (tracer_.empty()) {
        throw std::invalid_argument("FillStatesCalculator tracer is not set.");
    }

    const TracerResources resources = getTracerResources();

    // Load mean/std and ROI images
    ImageType::Pointer meanImage = Common::LoadNii(resources.meanPath);
    ImageType::Pointer stdImage  = Common::LoadNii(resources.stdPath);

    ImageType::Pointer roiImage;
    if (!resources.roiPath.empty()) {
        roiImage = Common::LoadNii(resources.roiPath);
    } else if (!resources.roiMaskKey.empty()) {
        roiImage = Common::LoadNii(config_->getMaskPath(resources.roiMaskKey));
    } else {
        throw std::runtime_error("No ROI specified for fill-states calculation.");
    }

    // Resample all auxiliary images to match the spatially normalized image
    ImageType::Pointer meanResampled = Common::ResampleToMatch(spatialNormalizedImage, meanImage);
    ImageType::Pointer stdResampled  = Common::ResampleToMatch(spatialNormalizedImage, stdImage);
    ImageType::Pointer roiResampled  = Common::ResampleToMatch(spatialNormalizedImage, roiImage);

    // Intensity normalization using tracer-specific reference ROI, if provided
    double refMean = 1.0;
    if (!resources.refMaskKey.empty()) {
        ImageType::Pointer refTemplate = Common::LoadNii(config_->getMaskPath(resources.refMaskKey));
        ImageType::Pointer imageInRefSpace = Common::ResampleToMatch(refTemplate, spatialNormalizedImage);
        refMean = Common::CalculateMeanInMask(imageInRefSpace, refTemplate);
        if (refMean <= 0.0) {
            throw std::runtime_error("Reference region mean is non-positive for fill-states.");
        }
    }

    // Prepare output mask image (0/1 float image in the same space)
    lastMaskImage_ = ImageType::New();
    lastMaskImage_->SetRegions(spatialNormalizedImage->GetLargestPossibleRegion());
    lastMaskImage_->SetSpacing(spatialNormalizedImage->GetSpacing());
    lastMaskImage_->SetOrigin(spatialNormalizedImage->GetOrigin());
    lastMaskImage_->SetDirection(spatialNormalizedImage->GetDirection());
    lastMaskImage_->Allocate();
    lastMaskImage_->FillBuffer(0.0f);

    itk::ImageRegionIterator<ImageType> itInput(spatialNormalizedImage,
                                                spatialNormalizedImage->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageType> itMean(meanResampled,
                                               meanResampled->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageType> itStd(stdResampled,
                                              stdResampled->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageType> itRoi(roiResampled,
                                              roiResampled->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageType> itMask(lastMaskImage_,
                                               lastMaskImage_->GetLargestPossibleRegion());

    const double threshold = 1.65;
    const std::string tracerLower = toLowerCopy(tracer_);

    std::size_t roiCount = 0;
    std::size_t positiveCount = 0;

    for (itInput.GoToBegin(), itMean.GoToBegin(), itStd.GoToBegin(),
         itRoi.GoToBegin(), itMask.GoToBegin();
         !itInput.IsAtEnd();
         ++itInput, ++itMean, ++itStd, ++itRoi, ++itMask) {

        const float roiVal = itRoi.Get();
        if (roiVal <= 0.0f) {
            itMask.Set(0.0f);
            continue;
        }

        const double sigma = static_cast<double>(itStd.Get());
        if (sigma <= 0.0) {
            itMask.Set(0.0f);
            continue;
        }

        double intensity = static_cast<double>(itInput.Get());
        if (refMean > 0.0) {
            intensity /= refMean;
        }
        const double mu        = static_cast<double>(itMean.Get());
        const double z         = (intensity - mu) / sigma;

        bool isPositive = false;
        if (tracerLower == "fdg") {
            // FDG: z < -1.65 indicates hypometabolism
            isPositive = (z < -threshold);
        } else {
            // FBP / FTP: z > 1.65
            isPositive = (z > threshold);
        }

        ++roiCount;
        if (isPositive) {
            ++positiveCount;
            itMask.Set(1.0f);
        } else {
            itMask.Set(0.0f);
        }
    }

    MetricResult result;
    result.metricName = "FillStates";
    result.suvr = 0.0; // Not defined for fill-states; reserved for future use.

    float fillStatesValue = 0.0f;
    if (roiCount > 0) {
        fillStatesValue =
            static_cast<float>(static_cast<double>(positiveCount) /
                               static_cast<double>(roiCount));
    }

    std::string tracerLabel;
    if (tracerLower == "fbp") {
        tracerLabel = "FBP";
    } else if (tracerLower == "fdg") {
        tracerLabel = "FDG";
    } else if (tracerLower == "ftp") {
        tracerLabel = "FTP";
    } else {
        tracerLabel = tracer_;
    }

    result.tracerValues[tracerLabel] = fillStatesValue;

    return result;
}

std::string FillStatesCalculator::getName() const {
    return "FillStates";
}

std::vector<std::string> FillStatesCalculator::getSupportedTracers() const {
    return {"fbp", "fdg", "ftp"};
}


