#include "CentiloidCLI.h"
#include "../../../cli/Options.h"
#include "../../interfaces/IMetricCLI.h"
#include "CentiloidLogic.h"
#include <memory>

namespace RefactorPipeline::Metrics::Centiloid {

namespace {

class CentiloidCLI : public IMetricCLI {
public:
    std::string getSubcommandName() const override {
        return "refactor-centiloid";
    }

    std::string getDescription() const override {
        return "Prototype Centiloid pipeline built with service/DI refactor";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addSUVrDerivedMetricArguments(parser);
    }

    void registerServices(ServiceContainer& container) override {
        registerMetric(container);
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        SUVrDerivedMetricOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.includeSUVr = parser.get<bool>("--suvr");
        options.skipRegistration = parser.get<bool>("--skip-normalization");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.useManualFOV = parser.get<bool>("--manual-fov");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        return runCommand(options, fullCommand);
    }
};

} // namespace

MetricCLIPtr createCLI() {
    return std::make_shared<CentiloidCLI>();
}

} // namespace RefactorPipeline::Metrics::Centiloid
