#include "cli/Commands.h"
#include "cli/Options.h"
#include "core/config/Version.h"
#include "metrics/ModuleCatalog.h"
#include "spatialNormalizations/ModuleCatalog.h"
#include <iostream>
#include <itkNiftiImageIOFactory.h>
#include <memory>
#include <vector>

int main(int argc, char* argv[]) {
    itk::NiftiImageIOFactory::RegisterOneFactory();
    
    // Reconstruct full command line for logging
    std::string fullCommand;
    if (argc > 0) {
        fullCommand = argv[0];
        for (int i = 1; i < argc; ++i) {
            fullCommand += " " + std::string(argv[i]);
        }
    }

    try {
        // Setup main program with subcommands
        argparse::ArgumentParser program("CentiloidCalculator", SOFTWARE_VERSION);
        program.add_description("PET image analysis toolkit for quantitative biomarker calculation");
        
        // Create subcommand parsers in main function to ensure proper lifetime
        // Centiloid subcommand
        argparse::ArgumentParser centiloid_cmd("centiloid");
        centiloid_cmd.add_description("Calculate Centiloid metric for amyloid PET images");
        addSUVrDerivedMetricArguments(centiloid_cmd);

        // CenTauR subcommand
        argparse::ArgumentParser centaur_cmd("centaur");
        centaur_cmd.add_description("Calculate CenTauR metric for tau PET images");
        addSUVrDerivedMetricArguments(centaur_cmd);

        // CenTauRz subcommand
        argparse::ArgumentParser centaurz_cmd("centaurz");
        centaurz_cmd.add_description("Calculate CenTauRz metric for tau PET images (z-score)");
        addSUVrDerivedMetricArguments(centaurz_cmd);

        // Fill-states subcommand
        argparse::ArgumentParser fillstates_cmd("fillstates");
        fillstates_cmd.add_description("Calculate fill-states metric for PET images");
        addFillStatesArguments(fillstates_cmd);

        // SUVr subcommand  
        argparse::ArgumentParser suvr_cmd("suvr");
        suvr_cmd.add_description("Calculate SUVr metric with custom VOI and reference masks");
        addBaseArguments(suvr_cmd);
        addSpatialNormalizationArguments(suvr_cmd);
        suvr_cmd.add_argument("--voi-mask")
            .help("VOI (Volume of Interest) mask path")
            .required();
        suvr_cmd.add_argument("--ref-mask")
            .help("Reference region mask path")
            .required();
        suvr_cmd.add_argument("--skip-normalization")
            .help("Skip spatial normalization")
            .default_value(false)
            .implicit_value(true);
        
        // Decouple subcommand
        argparse::ArgumentParser decouple_cmd("decouple");
        decouple_cmd.add_description("Decouple PET images to extract AD-related components");
        addBaseArguments(decouple_cmd);
        addSpatialNormalizationArguments(decouple_cmd);
        decouple_cmd.add_argument("--modality")
            .help("Modality to decouple")
            .choices("abeta", "tau")
            .required();
        decouple_cmd.add_argument("--skip-normalization")
            .help("Skip spatial normalization")
            .default_value(false)
            .implicit_value(true);
        
        // Refactored CLI modules (plugin-style)
        auto refactorModules = RefactorPipeline::Metrics::buildCLIModules();
        std::vector<std::unique_ptr<argparse::ArgumentParser>> refactorModuleParsers;
        std::vector<std::string> refactorModuleNames;
        refactorModuleParsers.reserve(refactorModules.size());
        refactorModuleNames.reserve(refactorModules.size());
        for (const auto& module : refactorModules) {
            auto parser = std::make_unique<argparse::ArgumentParser>(module->getSubcommandName());
            parser->add_description(module->getDescription());
            module->configureArguments(*parser);
            program.add_subparser(*parser);
            refactorModuleNames.push_back(module->getSubcommandName());
            refactorModuleParsers.push_back(std::move(parser));
        }
        
        // Add subcommands to main program
        program.add_subparser(centiloid_cmd);
        program.add_subparser(centaur_cmd);
        program.add_subparser(centaurz_cmd);
        program.add_subparser(fillstates_cmd);
        program.add_subparser(suvr_cmd);
        program.add_subparser(decouple_cmd);
        
        auto spatialModules = RefactorPipeline::SpatialNormalization::buildCLIModules();
        std::vector<std::unique_ptr<argparse::ArgumentParser>> spatialModuleParsers;
        std::vector<std::string> spatialModuleNames;
        spatialModuleParsers.reserve(spatialModules.size());
        spatialModuleNames.reserve(spatialModules.size());
        for (const auto& module : spatialModules) {
            auto parser = std::make_unique<argparse::ArgumentParser>(module->getSubcommandName());
            parser->add_description(module->getDescription());
            module->configureArguments(*parser);
            program.add_subparser(*parser);
            spatialModuleNames.push_back(module->getSubcommandName());
            spatialModuleParsers.push_back(std::move(parser));
        }
        
        program.parse_args(argc, argv);
        
        // Execute appropriate subcommand
        if (program.is_subcommand_used("centiloid")) {
            return executeCentiloidCommand(centiloid_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaur")) {
            return executeCenTauRCommand(centaur_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaurz")) {
            return executeCenTauRzCommand(centaurz_cmd, fullCommand);
        } else if (program.is_subcommand_used("fillstates")) {
            return executeFillStatesCommand(fillstates_cmd, fullCommand);
        } else if (program.is_subcommand_used("suvr")) {
            return executeSUVrCommand(suvr_cmd, fullCommand);
        } else if (program.is_subcommand_used("decouple")) {
            return executeDecoupleCommand(decouple_cmd);
        }
        
        for (size_t i = 0; i < spatialModules.size(); ++i) {
            if (program.is_subcommand_used(spatialModuleNames[i])) {
                return spatialModules[i]->execute(*spatialModuleParsers[i], fullCommand);
            }
        }
        
        for (size_t i = 0; i < refactorModules.size(); ++i) {
            if (program.is_subcommand_used(refactorModuleNames[i])) {
                return refactorModules[i]->execute(*refactorModuleParsers[i], fullCommand);
            }
        }
        
        std::cerr << "No subcommand specified. Use --help for usage information." << std::endl;
        std::cerr << program;
        return EXIT_FAILURE;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
