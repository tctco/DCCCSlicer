#pragma once
#include "./ProcessingContracts.h"
#include "../../interfaces/IMetricCalculator.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace RefactorPipeline::BatchLogging {

struct CsvContext {
    std::ofstream stream;
    bool headerWritten = false;
    std::vector<std::string> tracerKeys;
};

struct BatchInfoContext {
    std::ofstream stream;
};

BatchInfoContext openBatchInfo(const std::filesystem::path& outputDir,
                               const std::string& commandLine,
                               const std::string& version,
                               const std::string& configPath,
                               const std::filesystem::path& inputDir);

void appendSuccessEntry(BatchInfoContext& ctx, const std::string& label);
void appendFailureEntry(BatchInfoContext& ctx, const std::string& label, const std::string& reason);
void finalizeBatchInfo(BatchInfoContext& ctx, const BatchProcessingSummary& summary);

CsvContext openCsv(const std::filesystem::path& outputDir);
void appendCsvRows(CsvContext& ctx, const std::string& filename, const std::vector<MetricResult>& results);

} // namespace RefactorPipeline::BatchLogging


