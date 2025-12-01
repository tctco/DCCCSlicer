#include <argparse/argparse.hpp>
#include "core/config/Version.h"
#include "metrics/ModuleCatalog.h"
#include "spatialNormalizations/ModuleCatalog.h"
#include <iostream>
#include <itkNiftiImageIOFactory.h>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string buildFullCommand(int argc, char* argv[]) {
    if (argc <= 0) {
        return {};
    }
    std::string command = argv[0];
    for (int i = 1; i < argc; ++i) {
        command += " " + std::string(argv[i]);
    }
    return command;
}

} // namespace

int main(int argc, char* argv[]) {
    itk::NiftiImageIOFactory::RegisterOneFactory();
    const std::string fullCommand = buildFullCommand(argc, argv);

    try {
        argparse::ArgumentParser program("DCCCcore", SOFTWARE_VERSION);
        program.add_description("PET image analysis toolkit for quantitative biomarker calculation");

        auto metricModules = Pipeline::Metrics::buildCLIModules();
        std::vector<std::unique_ptr<argparse::ArgumentParser>> metricParsers;
        std::vector<std::string> metricNames;
        metricParsers.reserve(metricModules.size());
        metricNames.reserve(metricModules.size());

        for (const auto& module : metricModules) {
            auto parser = std::make_unique<argparse::ArgumentParser>(module->getSubcommandName());
            parser->add_description(module->getDescription());
            module->configureArguments(*parser);
            program.add_subparser(*parser);
            metricNames.push_back(module->getSubcommandName());
            metricParsers.push_back(std::move(parser));
        }

        auto spatialModules = Pipeline::SpatialNormalization::buildCLIModules();
        std::vector<std::unique_ptr<argparse::ArgumentParser>> spatialParsers;
        std::vector<std::string> spatialNames;
        spatialParsers.reserve(spatialModules.size());
        spatialNames.reserve(spatialModules.size());

        for (const auto& module : spatialModules) {
            auto parser = std::make_unique<argparse::ArgumentParser>(module->getSubcommandName());
            parser->add_description(module->getDescription());
            module->configureArguments(*parser);
            program.add_subparser(*parser);
            spatialNames.push_back(module->getSubcommandName());
            spatialParsers.push_back(std::move(parser));
        }

        program.parse_args(argc, argv);

        for (size_t i = 0; i < spatialModules.size(); ++i) {
            if (program.is_subcommand_used(spatialNames[i])) {
                return spatialModules[i]->execute(*spatialParsers[i], fullCommand);
            }
        }

        for (size_t i = 0; i < metricModules.size(); ++i) {
            if (program.is_subcommand_used(metricNames[i])) {
                return metricModules[i]->execute(*metricParsers[i], fullCommand);
            }
        }

        std::cerr << "No subcommand specified. Use --help for usage information." << std::endl;
        std::cerr << program;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
