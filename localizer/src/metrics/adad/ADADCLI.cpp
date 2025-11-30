#include "ADADCLI.h"
#include "../../core/interfaces/IMetricCLI.h"
#include "ADADLogic.h"
#include <memory>
#include <string>

namespace RefactorPipeline::Metrics::ADAD {

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

class ADADCLI : public IMetricCLI {
public:
    std::string getSubcommandName() const override {
        return "refactor-adad";
    }

    std::string getDescription() const override {
        return "Prototype ADAD scoring pipeline built with service/DI refactor";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addBaseArguments(parser);
        addSpatialNormalizationArguments(parser);
        parser.add_argument("--skip-normalization")
            .help("Skip the spatial normalization stage")
            .default_value(false)
            .implicit_value(true);
        parser.add_argument("--modality")
            .help("Decoupling modality to use (abeta or tau)")
            .default_value(std::string("abeta"))
            .choices("abeta", "tau");
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        ADADCLIOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        options.skipRegistration = parser.get<bool>("--skip-normalization");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.useManualFOV = parser.get<bool>("--manual-fov");
        options.modality = parser.get<std::string>("--modality");
        return runCommand(options, fullCommand);
    }
};

} // namespace

MetricCLIPtr createCLI() {
    return std::make_shared<ADADCLI>();
}

} // namespace RefactorPipeline::Metrics::ADAD

