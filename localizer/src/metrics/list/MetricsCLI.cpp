#include "MetricsCLI.h"

#include "../../core/di/Bootstrap.h"
#include "../../core/interfaces/IMetricModuleRegistry.h"
#include <argparse/argparse.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace Pipeline::Metrics::List {

namespace {

struct MetricsCLIOptions {
    std::string configPath;
    bool enableDebugOutput = false;
};

int runCommand(const MetricsCLIOptions& options);

void addArguments(argparse::ArgumentParser& parser) {
    parser.add_argument("--config")
        .help("Configuration file path")
        .default_value(std::string{"config.toml"});
    parser.add_argument("--debug")
        .help("Enable debug output")
        .default_value(false)
        .implicit_value(true);
}

class MetricsCLI : public IMetricCLI {
public:
    std::string getSubcommandName() const override {
        return "metrics";
    }

    std::string getDescription() const override {
        return "List the registered metric modules";
    }

    void configureArguments(argparse::ArgumentParser& parser) override {
        addArguments(parser);
    }

    int execute(const argparse::ArgumentParser& parser,
                const std::string& fullCommand) override {
        (void)fullCommand;
        MetricsCLIOptions options;
        options.configPath = parser.get<std::string>("--config");
        options.enableDebugOutput = parser.get<bool>("--debug");
        return runCommand(options);
    }
};

int runCommand(const MetricsCLIOptions& options) {
    BootstrapOptions bootstrapOptions;
    bootstrapOptions.configPath = options.configPath;
    bootstrapOptions.enableConfigDebug = options.enableDebugOutput;
    bootstrapOptions.logTag = "metrics";
    auto container = buildDefaultContainer(bootstrapOptions);
    auto registry = container->resolve<IMetricModuleRegistry>();
    auto modules = registry->listModules();
    if (modules.empty()) {
        std::cout << "No registered metrics available." << std::endl;
        return EXIT_SUCCESS;
    }

    std::sort(modules.begin(), modules.end());
    std::cout << "Available metrics:" << std::endl;
    for (const auto& moduleName : modules) {
        std::cout << "  " << moduleName << std::endl;
    }
    return EXIT_SUCCESS;
}

} // namespace

MetricCLIPtr createCLI() {
    return std::make_shared<MetricsCLI>();
}

} // namespace Pipeline::Metrics::List
