#pragma once

#include <chrono>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>

namespace Common::debug {

class DebugReporter;

class ScopedStage {
public:
    ScopedStage();
    ScopedStage(DebugReporter* reporter, std::string label, std::string details = "");
    ScopedStage(const ScopedStage&) = delete;
    ScopedStage& operator=(const ScopedStage&) = delete;
    ScopedStage(ScopedStage&& other) noexcept;
    ScopedStage& operator=(ScopedStage&& other) noexcept;
    ~ScopedStage();

private:
    DebugReporter* reporter_;
    std::string label_;
    std::chrono::steady_clock::time_point startedAt_;
    bool active_;
};

class DebugReporter {
public:
    explicit DebugReporter(std::string component, std::ostream& output);
    explicit DebugReporter(std::string component);

    void event(const std::string& label, const std::string& details = "") const;
    ScopedStage scope(const std::string& label, const std::string& details = "");

private:
    friend class ScopedStage;

    using Clock = std::chrono::steady_clock;

    void beginStage(const std::string& label, const std::string& details) const;
    void endStage(const std::string& label, Clock::time_point startedAt) const;
    double elapsedMilliseconds(Clock::time_point at) const;
    static std::string formatMilliseconds(double value);
    void write(const std::string& label, const std::string& details) const;

    std::string component_;
    Clock::time_point startedAt_;
    std::ostream* output_;
    mutable std::mutex mutex_;
};

using DebugReporterPtr = std::shared_ptr<DebugReporter>;

}  // namespace Common::debug
