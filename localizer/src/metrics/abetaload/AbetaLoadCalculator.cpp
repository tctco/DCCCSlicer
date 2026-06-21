#include "AbetaLoadCalculator.h"
#include "../../core/common/Common.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

AbetaLoadCalculator::AbetaLoadCalculator(ConfigurationPtr config)
    : config_(std::move(config)) {}

AbetaLoadCalculator::TemplatePaths
AbetaLoadCalculator::getTemplatePaths() const {
  TemplatePaths paths;
  paths.nsPath = config_->getTemplatePath("abeta_ns");
  paths.kPath = config_->getTemplatePath("abeta_k");
  return paths;
}

Pipeline::Metrics::MetricResult
AbetaLoadCalculator::calculate(ImageType::Pointer spatialNormalizedImage) {
  if (!spatialNormalizedImage) {
    throw std::invalid_argument(
        "AbetaLoadCalculator requires a spatially normalized image.");
  }

  const auto templates = getTemplatePaths();
  // Intensity normalization using whole_cerebral reference
  ImageType::Pointer refMask =
      Common::nifti::loadImage(config_->getMaskPath("whole_cerebral"));
  if (!refMask) {
    throw std::runtime_error(
        "Failed to load reference mask for AbetaLoad normalization.");
  }
  ImageType::Pointer imageInRefSpace =
      Common::image::resampleToMatch(refMask, spatialNormalizedImage);
  double refMean = Common::image::calculateMeanInMask(imageInRefSpace, refMask);
  if (refMean <= 0.0) {
    throw std::runtime_error(
        "Reference mean is non-positive for AbetaLoad normalization.");
  }
  ImageType::Pointer nsTemplate = Common::nifti::loadImage(templates.nsPath);
  ImageType::Pointer kTemplate = Common::nifti::loadImage(templates.kPath);

  if (!nsTemplate || !kTemplate) {
    throw std::runtime_error(
        "Failed to load NS/K templates for AbetaLoad calculation.");
  }

  // Align templates to the subject space
  ImageType::Pointer nsResampled =
      Common::image::resampleToMatch(spatialNormalizedImage, nsTemplate);
  ImageType::Pointer kResampled =
      Common::image::resampleToMatch(spatialNormalizedImage, kTemplate);

  std::vector<float> targetData;
  std::vector<float> nsData;
  std::vector<float> kData;
  Common::image::extractImageData(spatialNormalizedImage, targetData);
  Common::image::extractImageData(nsResampled, nsData);
  Common::image::extractImageData(kResampled, kData);

  for (auto &v : targetData) {
    v = static_cast<float>(v / refMean);
  }

  if (nsData.size() != kData.size() || targetData.size() != nsData.size()) {
    throw std::runtime_error(
        "AbetaLoad inputs have mismatched dimensions after resampling.");
  }

  const double epsilon = 1e-8;
  double sumNN = 0.0;
  double sumKK = 0.0;
  double sumNK = 0.0;
  double sumNy = 0.0;
  double sumKy = 0.0;
  std::size_t validCount = 0;

  for (std::size_t i = 0; i < targetData.size(); ++i) {
    const double n = static_cast<double>(nsData[i]);
    const double k = static_cast<double>(kData[i]);
    const double y = static_cast<double>(targetData[i]);

    if (!std::isfinite(n) || !std::isfinite(k) || !std::isfinite(y)) {
      continue;
    }
    if (std::abs(n) < epsilon && std::abs(k) < epsilon) {
      continue;
    }

    sumNN += n * n;
    sumKK += k * k;
    sumNK += n * k;
    sumNy += n * y;
    sumKy += k * y;
    ++validCount;
  }

  if (validCount == 0) {
    throw std::runtime_error(
        "No valid voxels found for AbetaLoad decomposition.");
  }

  // Solve for coefficients (nsCoeff, kCoeff) in a two-component non-negative LS
  // sense: minimize || y - nsCoeff*N - kCoeff*K ||^2. The normal equations give
  // a 2x2 system: [sumNN sumNK; sumNK sumKK] * [nsCoeff kCoeff]^T = [sumNy
  // sumKy]^T. We solve the system when well-conditioned; otherwise fall back to
  // single-channel ratios.
  const double denom = sumNN * sumKK - sumNK * sumNK;
  double nsCoeff = 0.0;
  double kCoeff = 0.0;

  if (std::abs(denom) > epsilon) {
    nsCoeff = (sumNy * sumKK - sumKy * sumNK) / denom;
    kCoeff = (sumKy * sumNN - sumNy * sumNK) / denom;
  } else {
    if (sumNN > epsilon) {
      nsCoeff = sumNy / sumNN;
    }
    if (sumKK > epsilon) {
      kCoeff = sumKy / sumKK;
    }
  }

  // Coefficients are expected to be non-negative
  nsCoeff = std::max(0.0, nsCoeff);
  kCoeff = std::max(0.0, kCoeff);

  Pipeline::Metrics::MetricResult result;
  result.metricName = "AbetaLoad";
  result.suvr = kCoeff;
  result.tracerValues["NS"] = static_cast<float>(nsCoeff);
  result.tracerValues["Abeta_load"] = static_cast<float>(kCoeff);
  return result;
}
