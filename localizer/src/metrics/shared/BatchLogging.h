#pragma once

#include "MetricTypes.h"
#include <rapidcsv.h>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace Pipeline::Metrics::Shared {

struct BatchSummary {
    size_t processed = 0;
    size_t succeeded = 0;
    size_t failed = 0;
};

struct CsvContext {
    std::filesystem::path outputPath;
    std::unique_ptr<rapidcsv::Document> document;
    bool headerWritten = false;
    std::vector<std::string> tracerKeys;
    size_t nextRowIndex = 0;
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
void finalizeBatchInfo(BatchInfoContext& ctx, const BatchSummary& summary);

CsvContext openCsv(const std::filesystem::path& outputDir);
void appendCsvRows(CsvContext& ctx,
                   const std::string& filename,
                   const std::vector<Pipeline::Metrics::MetricResult>& results);

} // namespace Pipeline::Metrics::Shared
