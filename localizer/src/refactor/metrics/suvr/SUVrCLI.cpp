#include "SUVrCLI.h"
#include "../../../cli/Options.h"
#include "../../interfaces/IMetricCLI.h"
#include "SUVrLogic.h"
#include <memory>

namespace RefactorPipeline::Metrics::SUVr {

namespace {

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

    void registerServices(ServiceContainer& container) override {
        registerMetric(container);
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        SUVrCommandOptions options;
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
