#pragma once
#include "ProcessingPipeline.h"
#include <string>
#include <functional>

class BatchProcessor {
public:
    // Function signature for processing a single file
    // Returns ProcessingResult which contains the metrics to be logged
    using SingleFileProcessor = std::function<ProcessingResult(const std::string& inputPath, const std::string& outputPath)>;

    /**
     * @brief Run batch processing on a directory of NIfTI files
     * 
     * @param inputDir Directory containing input .nii files
     * @param outputDir Directory to save outputs
     * @param configPath Path to configuration file used
     * @param version Software version string
     * @param commandLine Full command line instruction
     * @param skipRegistration Whether registration is skipped (affects output dir validation)
     * @param processor Callback function to process each file
     * @return EXIT_SUCCESS or EXIT_FAILURE
     */
    static int runBatch(
        const std::string& inputDir,
        const std::string& outputDir,
        const std::string& configPath,
        const std::string& version,
        const std::string& commandLine,
        bool skipRegistration,
        SingleFileProcessor processor
    );
};

