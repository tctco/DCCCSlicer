#include "SUVrCLI.h"
#include "../../core/interfaces/IMetricCLI.h"
#include "SUVrLogic.h"
#include <memory>
#include <string>

namespace RefactorPipeline::Metrics::SUVr {

namespace {

void addBaseArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("--input")
        .help("Input PET image path")
        .required();
    parser.add_argument("--output")
        .help("Output processed image path")
        .required();
    parser.add_argument("--config")
        .help("Configuration file path")
        .default_value(std::string{"config.toml"});
    parser.add_argument("--debug")
        .help("Enable debug mode")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--batch")
        .help("Enable batch processing mode")
        .default_value(false)
        .implicit_value(true);
}

void addSpatialNormalizationArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("-i", "--iterative")
        .help("Use iterative rigid transformation")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("-m", "--manual-fov")
        .help("Use manual FOV placement")
        .default_value(false)
        .implicit_value(true);
}

class SUVrCLI : public IMetricCLI {
public:
    std::string getSubcommandName() const override {
        return "refactor-suvr";
    }

    std::string getDescription() const override {
        return "Prototype SUVR pipeline built with service/DI refactor";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
        addSpatialNormalizationArguments(parser);
        parser.add_argument("--skip-normalization")
            .help("Skip the spatial normalization stage")
            .default_value(false)
            .implicit_value(true);
        parser.add_argument("--voi-mask")
            .help("Absolute path to the VOI mask")
            .default_value(std::string{});
        parser.add_argument("--ref-mask")
            .help("Absolute path to the reference mask")
            .default_value(std::string{});
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        SUVrCLIOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        options.skipRegistration = parser.get<bool>("--skip-normalization");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.useManualFOV = parser.get<bool>("--manual-fov");
        options.voiMaskPath = parser.get<std::string>("--voi-mask");
        options.refMaskPath = parser.get<std::string>("--ref-mask");
        return runCommand(options, fullCommand);
    }
};

} // namespace

MetricCLIPtr createCLI() {
    return std::make_shared<SUVrCLI>();
}

} // namespace RefactorPipeline::Metrics::SUVr
