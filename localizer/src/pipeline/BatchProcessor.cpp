#include "BatchProcessor.h"
#include "../utils/common.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <set>

namespace fs = std::filesystem;

// Helper to get current time string
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Helper to check if string ends with suffix (C++17 compatible)
bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

int BatchProcessor::runBatch(
    const std::string& inputDir,
    const std::string& outputDir,
    const std::string& configPath,
    const std::string& version,
    const std::string& commandLine,
    bool skipRegistration,
    SingleFileProcessor processor
) {
    // 1. Validate directories
    if (!fs::exists(inputDir)) {
        std::cerr << "Error: Input directory does not exist: " << inputDir << std::endl;
        return EXIT_FAILURE;
    }
    if (!fs::is_directory(inputDir)) {
        std::cerr << "Error: Input path is not a directory: " << inputDir << std::endl;
        return EXIT_FAILURE;
    }

    if (!fs::exists(outputDir)) {
        try {
            fs::create_directories(outputDir);
        } catch (const std::exception& e) {
            std::cerr << "Error: Could not create output directory: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    } else if (!fs::is_directory(outputDir)) {
        std::cerr << "Error: Output path is not a directory: " << outputDir << std::endl;
        return EXIT_FAILURE;
    }

    // Check if output directory is empty when registration is not skipped
    if (!skipRegistration && !fs::is_empty(outputDir)) {
        std::cerr << "Error: Output directory must be empty when registration is enabled to avoid overwriting." << std::endl;
        return EXIT_FAILURE;
    }

    // 2. Scan files
    std::vector<fs::path> inputFiles;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // Use manual check instead of ends_with for C++17 compatibility
            std::string pathStr = entry.path().string();
            if (ext == ".nii" || endsWith(pathStr, ".nii.gz")) {
                inputFiles.push_back(entry.path());
            }
        }
    }

    if (inputFiles.empty()) {
        std::cerr << "Warning: No .nii or .nii.gz files found in " << inputDir << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "Found " << inputFiles.size() << " files to process." << std::endl;

    // 3. Initialize batch info
    std::string batchInfoPath = (fs::path(outputDir) / "batch_info.txt").string();
    std::ofstream batchInfo(batchInfoPath);
    std::string startTime = getCurrentTime();
    
    batchInfo << "Software Version: " << version << std::endl;
    batchInfo << "Command: " << commandLine << std::endl;
    batchInfo << "Start Time: " << startTime << std::endl;
    batchInfo << "Config Path: " << configPath << std::endl;
    batchInfo << "Input Directory: " << inputDir << std::endl;
    batchInfo << "Output Directory: " << outputDir << std::endl;
    batchInfo.flush();

    // 4. Process files
    std::string csvPath = (fs::path(outputDir) / "results.csv").string();
    std::ofstream csvFile(csvPath);
    bool csvHeaderWritten = false;
    std::vector<std::string> tracerKeys;

    int successCount = 0;
    int failCount = 0;

    for (const auto& inputFile : inputFiles) {
        std::cout << "Processing [" << (successCount + failCount + 1) << "/" << inputFiles.size() << "]: " << inputFile.filename() << std::endl;
        
        std::string inputPath = inputFile.string();
        
        // Construct output image path (preserve filename but ensure .nii extension if needed)
        std::string outputFilename = inputFile.stem().string();
        // If .nii.gz, stem() removes .gz, getting .nii is tricky with std::filesystem sometimes if double extension
        // But simple stem() is usually fine. If filename is "image.nii", stem is "image".
        // If "image.nii.gz", stem is "image.nii".
        if (endsWith(inputFile.string(), ".nii.gz")) {
             outputFilename = inputFile.stem().stem().string(); // Remove .gz then .nii
        } else if (inputFile.extension() == ".nii") {
             outputFilename = inputFile.stem().string();
        }
        
        std::string outputImagePath = (fs::path(outputDir) / (outputFilename + "_processed.nii")).string();

        try {
            ProcessingResult result = processor(inputPath, outputImagePath);

            // Write CSV header if needed
            if (!csvHeaderWritten && !result.metricResults.empty()) {
                csvFile << "Filename,Metric";
                
                // Collect all unique tracer keys from the first result
                std::set<std::string> keys;
                for (const auto& mr : result.metricResults) {
                    for (const auto& pair : mr.tracerValues) {
                        keys.insert(pair.first);
                    }
                }
                tracerKeys.assign(keys.begin(), keys.end());
                
                for (const auto& key : tracerKeys) {
                    csvFile << "," << key;
                }
                csvFile << ",SUVr"; // SUVr is standard
                csvFile << "\n";
                csvHeaderWritten = true;
            }

            // Write CSV row(s)
            for (const auto& mr : result.metricResults) {
                csvFile << inputFile.filename().string() << "," << mr.metricName;
                
                // Write tracer values in order of keys
                for (const auto& key : tracerKeys) {
                    auto it = mr.tracerValues.find(key);
                    if (it != mr.tracerValues.end()) {
                        csvFile << "," << it->second;
                    } else {
                        csvFile << ","; // Empty if metric doesn't have this tracer
                    }
                }
                
                csvFile << "," << mr.suvr << "\n";
            }
            csvFile.flush();
            
            successCount++;
        } catch (const std::exception& e) {
            std::cerr << "Failed to process " << inputFile.filename() << ": " << e.what() << std::endl;
            batchInfo << "Failed: " << inputFile.filename() << " - " << e.what() << std::endl;
            failCount++;
        }
    }

    // 5. Finalize batch info
    std::string endTime = getCurrentTime();
    batchInfo << "End Time: " << endTime << std::endl;
    batchInfo << "Processed: " << successCount << ", Failed: " << failCount << std::endl;
    
    std::cout << "\nBatch processing complete." << std::endl;
    std::cout << "Success: " << successCount << ", Failed: " << failCount << std::endl;
    std::cout << "Results saved to: " << outputDir << std::endl;

    return (failCount == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
