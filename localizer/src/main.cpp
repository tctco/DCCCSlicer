#include "cli/Commands.h"
#include "cli/Options.h"
#include "config/Version.h"
#include <iostream>
#include <itkNiftiImageIOFactory.h>

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
        
        // Normalize subcommand
        argparse::ArgumentParser normalize_cmd("normalize");
        normalize_cmd.add_description("Perform spatial normalization on PET images");
        addBaseArguments(normalize_cmd);
        addSpatialNormalizationArguments(normalize_cmd);
        normalize_cmd.add_argument("--method")
            .help("Normalization method")
            .default_value("rigid_voxelmorph");
        normalize_cmd.add_argument("--ADNI-PET-core")
            .help("Enable ADNI PET core style processing")
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
        
        // Add subcommands to main program
        program.add_subparser(centiloid_cmd);
        program.add_subparser(centaur_cmd);
        program.add_subparser(centaurz_cmd);
        program.add_subparser(suvr_cmd);
        program.add_subparser(normalize_cmd);
        program.add_subparser(decouple_cmd);
        
        program.parse_args(argc, argv);
        
        // Execute appropriate subcommand
        if (program.is_subcommand_used("centiloid")) {
            return executeCentiloidCommand(centiloid_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaur")) {
            return executeCenTauRCommand(centaur_cmd, fullCommand);
        } else if (program.is_subcommand_used("centaurz")) {
            return executeCenTauRzCommand(centaurz_cmd, fullCommand);
        } else if (program.is_subcommand_used("suvr")) {
            return executeSUVrCommand(suvr_cmd, fullCommand);
        } else if (program.is_subcommand_used("normalize")) {
            return executeNormalizeCommand(normalize_cmd);
        } else if (program.is_subcommand_used("decouple")) {
            return executeDecoupleCommand(decouple_cmd);
        } else {
            std::cerr << "No subcommand specified. Use --help for usage information." << std::endl;
            std::cerr << program;
            return EXIT_FAILURE;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
