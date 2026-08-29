#include "PetMotionCorrectionCLI.h"

#include "../../core/common/Filesystem.h"
#include "../../core/preprocessing/PetMotionCorrector.h"
#include <exception>
#include <iostream>

namespace Pipeline::SpatialNormalization::Pet {

namespace {

class PetMotionCorrectionCLI final : public ISpatialNormalizationCLI {
public:
    std::string getSubcommandName() const override {
        return "pet-motion-correct";
    }

    std::string getDescription() const override {
        return "Rigidly align 4D PET frames to frame 0 and calculate their mean";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        parser.add_argument("--input")
            .help("Input multi-frame/4D PET NIfTI path")
            .required();
        parser.add_argument("--output")
            .help("Output averaged 3D float NIfTI path")
            .required();
        parser.add_argument("--save-corrected-dynamic")
            .help("Optional output path for the corrected 4D PET")
            .default_value(std::string{});
        parser.add_argument("--motion-output")
            .help("Optional TSV output path for per-frame rigid motion parameters")
            .default_value(std::string{});
    }

    int execute(const argparse::ArgumentParser& parser, const std::string&) override {
        const auto inputPath = parser.get<std::string>("--input");
        const auto outputPath = parser.get<std::string>("--output");
        const auto correctedPath = parser.get<std::string>("--save-corrected-dynamic");
        const auto motionPath = parser.get<std::string>("--motion-output");

        if (!Common::fs::ensureParentDirectory(outputPath) ||
            (!correctedPath.empty() && !Common::fs::ensureParentDirectory(correctedPath)) ||
            (!motionPath.empty() && !Common::fs::ensureParentDirectory(motionPath))) {
            std::cerr << "[pet-motion-correct] Failed to prepare an output directory." << std::endl;
            return EXIT_FAILURE;
        }

        try {
            Pipeline::Preprocessing::PetMotionCorrector corrector;
            auto result = corrector.correct(inputPath, !correctedPath.empty());
            Pipeline::Preprocessing::PetMotionCorrector::saveAveragedImage(result, outputPath);
            if (!correctedPath.empty()) {
                Pipeline::Preprocessing::PetMotionCorrector::saveCorrectedDynamicImage(
                    result, correctedPath);
            }
            if (!motionPath.empty()) {
                Pipeline::Preprocessing::PetMotionCorrector::saveMotionParameters(result, motionPath);
            }
            std::cout << "[pet-motion-correct] Corrected " << result.motion.size()
                      << " frame(s); averaged 3D PET saved to " << outputPath << std::endl;
            return EXIT_SUCCESS;
        } catch (const std::exception& ex) {
            std::cerr << "[pet-motion-correct] Processing failed: " << ex.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
};

} // namespace

SpatialNormalizationCLIPtr createMotionCorrectionCLI() {
    return std::make_shared<PetMotionCorrectionCLI>();
}

} // namespace Pipeline::SpatialNormalization::Pet
