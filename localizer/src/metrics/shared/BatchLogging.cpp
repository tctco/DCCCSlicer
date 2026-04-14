#include "BatchLogging.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace Pipeline::Metrics::Shared {

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

void finalizeBatchInfo(BatchInfoContext& ctx, const BatchSummary& summary) {
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
    ctx.outputPath = outputDir / "results.csv";
    ctx.document =
        std::make_unique<rapidcsv::Document>(std::string{}, rapidcsv::LabelParams(0, -1));
    return ctx;
}

void appendCsvRows(CsvContext& ctx,
                   const std::string& filename,
                   const std::vector<Pipeline::Metrics::MetricResult>& results) {
    if (!ctx.document || results.empty()) {
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
        size_t col = 0;
        ctx.document->SetColumnName(col++, "Filename");
        ctx.document->SetColumnName(col++, "Metric");
        for (const auto& key : ctx.tracerKeys) {
            ctx.document->SetColumnName(col++, key);
        }
        ctx.document->SetColumnName(col, "SUVr");
        ctx.headerWritten = true;
    }

    for (const auto& metric : results) {
        const size_t row = ctx.nextRowIndex++;
        ctx.document->SetCell("Filename", row, filename);
        ctx.document->SetCell("Metric", row, metric.metricName);
        for (const auto& key : ctx.tracerKeys) {
            auto it = metric.tracerValues.find(key);
            if (it != metric.tracerValues.end()) {
                ctx.document->SetCell(key, row, it->second);
            }
        }
        ctx.document->SetCell("SUVr", row, metric.suvr);
    }
    ctx.document->Save(ctx.outputPath.string());
}

} // namespace Pipeline::Metrics::Shared
