#pragma once
#include <itkImage.h>

#include "../common/ImageTypes.h"
#include "../interfaces/IConfiguration.h"
#include "SegmentationBundle.h"

namespace Segmentation {
using LabelImage = itk::Image<short, 3>;
struct Options {
    std::string mode = "freesurfer";
    int threads = 4;
    bool tta = true;
    std::string rigidHeader;
    std::string debugPrefix;
};
// Float buffers use ITK/SimpleITK contiguous X-fastest storage, i.e. C,D,H,W.
std::vector<short> predict(const std::vector<float>& normalized, const std::array<int, 3>& cropXYZ,
                           const Bundle& bundle, const Options& options);
class PetSegmenter {
   public:
    explicit PetSegmenter(ConfigurationPtr config) : config_(std::move(config)) {}
    LabelImage::Pointer segment(const std::string& input, const Bundle& bundle,
                                const Options& options) const;

   private:
    ConfigurationPtr config_;
};
void saveLabels(LabelImage::Pointer image, const std::string& path);
}  // namespace Segmentation
