#include <argparse/argparse.hpp>
#include "core/common/PathUtils.h"
#include "core/config/Version.h"
#include "metrics/ModuleCatalog.h"
#include "spatialNormalizations/ModuleCatalog.h"
#include <iostream>
#include <itkNiftiImageIOFactory.h>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#endif

namespace {

std::string buildFullCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {};
    }
    std::string command = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        command += " " + args[i];
    }
    return command;
}

#ifdef _WIN32
std::vector<std::string> collectUtf8Args(int argc, char* argv[]) {
    int wideArgc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (!wideArgv) {
        std::vector<std::string> fallback;
        fallback.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            fallback.emplace_back(argv[i]);
        }
        return fallback;
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(wideArgc));
    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(Common::path::wideToUtf8(wideArgv[i]));
    }
    LocalFree(wideArgv);
    return args;
}
#else
std::vector<std::string> collectUtf8Args(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    itk::NiftiImageIOFactory::RegisterOneFactory();
    std::vector<std::string> utf8Args = collectUtf8Args(argc, argv);
    std::vector<char*> argparseArgv;
    argparseArgv.reserve(utf8Args.size());
    for (auto& arg : utf8Args) {
        argparseArgv.push_back(arg.data());
    }

    const int normalizedArgc = static_cast<int>(argparseArgv.size());
    char** normalizedArgv = argparseArgv.data();
    const std::string fullCommand = buildFullCommand(utf8Args);

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

        program.parse_args(normalizedArgc, normalizedArgv);

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
