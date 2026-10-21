#include "SegCLI.h"

#include <filesystem>
#include <iostream>

#include "../../core/common/PathUtils.h"
#include "../../core/config/ConfigLoader.h"
#include "../../core/segmentation/PetSegmenter.h"
#include "../CLIOptions.h"

namespace Pipeline::SpatialNormalization::Seg {
namespace {
class CLI final : public ISpatialNormalizationCLI {
   public:
    std::string getSubcommandName() const override { return "seg"; }
    std::string getDescription() const override {
        return "Segment native PET with DCCC Seg (96 classes); NeuroSegmenter pons/brainstem IDs "
               "45/46 unavailable";
    }
    void configureArguments(argparse::ArgumentParser& p) override {
        addBaseArguments(p);
        p.add_argument("--mode")
            .default_value(std::string("freesurfer"))
            .help("Label mode: freesurfer, training, neurosegmenter (no pons)");
        p.add_argument("--threads")
            .default_value(4)
            .scan<'i', int>()
            .help("Positive CPU thread count (default 4)");
        p.add_argument("--no-tta")
            .default_value(false)
            .implicit_value(true)
            .help("Disable default 8-way mirror TTA; changes inference protocol");
        p.add_argument("--bundle")
            .default_value(std::string{})
            .help("Override installed segmentation bundle directory");
        p.add_argument("--rigid-header")
            .default_value(std::string{})
            .help("Validated two-step rigid header preserving native voxel values");
    }
    int execute(const argparse::ArgumentParser& p, const std::string&) override {
        try {
            if (p.get<bool>("--batch") || !p.get<std::string>("--bids").empty())
                throw std::invalid_argument("seg: --batch and --bids are not supported");
            Segmentation::Options options;
            options.mode = p.get<std::string>("--mode");
            options.threads = p.get<int>("--threads");
            options.tta = !p.get<bool>("--no-tta");
            options.rigidHeader = p.get<std::string>("--rigid-header");
            if (options.mode != "freesurfer" && options.mode != "training" &&
                options.mode != "neurosegmenter")
                throw std::invalid_argument("Unknown seg mode: " + options.mode);
            if (options.threads < 1) throw std::invalid_argument("threads must be positive");
            const auto output = Common::path::fromUtf8(p.get<std::string>("--output"));
            if (std::filesystem::exists(output) || std::filesystem::is_symlink(output))
                throw std::runtime_error("Refusing to overwrite existing output: " +
                                         Common::path::toUtf8(output));
            if (!output.parent_path().empty() &&
                !std::filesystem::is_directory(output.parent_path()))
                throw std::invalid_argument("Output parent directory must exist");
            BaseCommandOptions base;
            base.outputPath = Common::path::toUtf8(output);
            base.enableDebugOutput = p.get<bool>("--debug");
            setupDebugOutput(base);
            options.debugPrefix = base.debugOutputBasePath;
            ConfigurationLoadOptions load;
            load.configPath = p.get<std::string>("--config");
            load.logTag = "seg";
            auto config = loadConfigurationWithLogging(load);
            if (config->getString("models.seg").empty())
                config->setString("models.seg", "assets/models/seg/dccc_seg.onnx");
            const auto custom = p.get<std::string>("--bundle");
            const auto directory =
                custom.empty() ? Common::path::fromUtf8(config->getModelPath("seg")).parent_path()
                               : Common::path::fromUtf8(custom);
            Segmentation::Bundle bundle(directory);
            if (options.mode == "neurosegmenter")
                std::cerr << "[seg] Partial NeuroSegmenter mapping: IDs 45/46 unavailable; pons "
                             "SUVR unsupported."
                          << std::endl;
            auto result = Segmentation::PetSegmenter(config).segment(p.get<std::string>("--input"),
                                                                     bundle, options);
            // Check again after inference; never intentionally overwrite an existing result.
            if (std::filesystem::exists(output) || std::filesystem::is_symlink(output))
                throw std::runtime_error("Output already exists");
            Segmentation::saveLabels(result, Common::path::toUtf8(output));
            std::cout << "[seg] Saved native labels: " << Common::path::toUtf8(output) << std::endl;
            return EXIT_SUCCESS;
        } catch (const std::exception& e) {
            std::cerr << "[seg] " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
};
}  // namespace
SpatialNormalizationCLIPtr createCLI() { return std::make_shared<CLI>(); }
}  // namespace Pipeline::SpatialNormalization::Seg
