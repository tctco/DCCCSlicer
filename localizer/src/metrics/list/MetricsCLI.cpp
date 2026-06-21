#include "MetricsCLI.h"

#include "../ModuleCatalog.h"
#include <argparse/argparse.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace Pipeline::Metrics::List {

namespace {

struct MetricsCLIOptions {
};

int runCommand(const MetricsCLIOptions& options);

void addArguments(argparse::ArgumentParser& parser) {
    (void)parser;
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
        (void)parser;
        (void)fullCommand;
        return runCommand(MetricsCLIOptions{});
    }
};

int runCommand(const MetricsCLIOptions& options) {
    (void)options;
    auto modules = listMetricNames();
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

void registerModule(MetricRegistry& registry) {
    registry.registerModule({"metrics", false, &createCLI});
}

} // namespace Pipeline::Metrics::List
