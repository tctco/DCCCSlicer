#include "BatchLogging.h"
#include "../common/ProcessingContracts.h"
#include "../config/Version.h"
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>

namespace Pipeline::BatchLogging {

namespace {

std::string currentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace

BatchInfoContext openBatchInfo(const std::filesystem::path& outputDir,
                               const std::string& commandLine,
                               const std::string& version,
                               const std::string& configPath,
                               const std::filesystem::path& inputDir) {
    BatchInfoContext ctx;
    ctx.stream.open(outputDir / "batch_info.txt", std::ios::out | std::ios::trunc);
    if (!ctx.stream.is_open()) {
        return ctx;
    }
    ctx.stream << "Software Version: " << version << '\n';
    ctx.stream << "Command: " << commandLine << '\n';
    ctx.stream << "Start Time: " << currentTimeString() << '\n';
    ctx.stream << "Config Path: " << configPath << '\n';
    ctx.stream << "Input Directory: " << inputDir.string() << '\n';
    ctx.stream << "Output Directory: " << outputDir.string() << '\n';
    ctx.stream.flush();
    return ctx;
}

void appendSuccessEntry(BatchInfoContext& ctx, const std::string& label) {
    if (!ctx.stream.is_open()) {
        return;
    }
    ctx.stream << "Succeeded: " << label << '\n';
    ctx.stream.flush();
}

void appendFailureEntry(BatchInfoContext& ctx, const std::string& label, const std::string& reason) {
    if (!ctx.stream.is_open()) {
        return;
    }
    ctx.stream << "Failed: " << label << " - " << reason << '\n';
    ctx.stream.flush();
}

void finalizeBatchInfo(BatchInfoContext& ctx, const BatchProcessingSummary& summary) {
    if (!ctx.stream.is_open()) {
        return;
    }
    ctx.stream << "End Time: " << currentTimeString() << '\n';
    ctx.stream << "Processed: " << summary.processed
               << ", Succeeded: " << summary.succeeded
               << ", Failed: " << summary.failed << '\n';
    ctx.stream.flush();
}

CsvContext openCsv(const std::filesystem::path& outputDir) {
    CsvContext ctx;
    ctx.stream.open(outputDir / "results.csv", std::ios::out | std::ios::trunc);
    return ctx;
}

void appendCsvRows(CsvContext& ctx, const std::string& filename, const std::vector<MetricResult>& results) {
    if (!ctx.stream.is_open() || results.empty()) {
        return;
    }

    if (!ctx.headerWritten) {
        std::set<std::string> keys;
        for (const auto& metric : results) {
            for (const auto& pair : metric.tracerValues) {
                keys.insert(pair.first);
            }
        }
        ctx.tracerKeys.assign(keys.begin(), keys.end());
        ctx.stream << "Filename,Metric";
        for (const auto& key : ctx.tracerKeys) {
            ctx.stream << "," << key;
        }
        ctx.stream << ",SUVr\n";
        ctx.headerWritten = true;
    }

    for (const auto& metric : results) {
        ctx.stream << filename << "," << metric.metricName;
        for (const auto& key : ctx.tracerKeys) {
            auto it = metric.tracerValues.find(key);
            if (it != metric.tracerValues.end()) {
                ctx.stream << "," << it->second;
            } else {
                ctx.stream << ",";
            }
        }
        ctx.stream << "," << metric.suvr << "\n";
    }
    ctx.stream.flush();
}

} // namespace Pipeline::BatchLogging


