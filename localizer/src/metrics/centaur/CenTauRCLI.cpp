#include "CenTauRCLI.h"

#include "../../core/di/Bootstrap.h"
#include "CenTauRService.h"
#include <memory>
#include <string>

namespace Pipeline::Metrics::Centaur {

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
    parser.add_argument("--bids")
        .help("Treat --input as a PET-BIDS dataset root and process PET files matching this regex")
        .default_value(std::string{});
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

void addSUVrDerivedArguments(argparse::ArgumentParser& parser) {
    addBaseArguments(parser);
    addSpatialNormalizationArguments(parser);
    parser.add_argument("--suvr")
        .help("Include SUVr values in the output")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--skip-normalization")
        .help("Skip spatial normalization and calculate metrics directly")
        .default_value(false)
        .implicit_value(true);
}

class CenTauRCLI : public IMetricCLI {
public:
    std::string getSubcommandName() const override {
        return "centaur";
    }

    std::string getDescription() const override {
        return "Prototype CenTauR pipeline built with service/DI refactor";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addSUVrDerivedArguments(parser);
    }

    int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) override {
        CenTauRCLIOptions options;
        options.inputPath = parser.get<std::string>("--input");
        options.outputPath = parser.get<std::string>("--output");
        options.configPath = parser.get<std::string>("--config");
        options.includeSUVr = parser.get<bool>("--suvr");
        options.skipRegistration = parser.get<bool>("--skip-normalization");
        options.useIterativeRigid = parser.get<bool>("--iterative");
        options.useManualFOV = parser.get<bool>("--manual-fov");
        options.enableDebugOutput = parser.get<bool>("--debug");
        options.batchMode = parser.get<bool>("--batch");
        options.bidsPattern = parser.get<std::string>("--bids");

        BootstrapOptions bootstrapOptions;
        bootstrapOptions.configPath = options.configPath;
        bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
        bootstrapOptions.logTag = "centaur";
        auto container = buildCoreContainer(bootstrapOptions);
        auto service = createService(*container);
        return service->run(options, fullCommand);
    }
};

} // namespace

MetricCLIPtr createCLI() {
    return std::make_shared<CenTauRCLI>();
}

} // namespace Pipeline::Metrics::Centaur

