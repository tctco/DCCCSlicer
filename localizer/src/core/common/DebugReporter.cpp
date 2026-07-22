#include "DebugReporter.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace Common::debug {

ScopedStage::ScopedStage()
    : reporter_(nullptr), startedAt_(), active_(false) {}

ScopedStage::ScopedStage(DebugReporter* reporter, std::string label, std::string details)
    : reporter_(reporter),
      label_(std::move(label)),
      startedAt_(DebugReporter::Clock::now()),
      active_(reporter_ != nullptr) {
    if (active_) {
        reporter_->beginStage(label_, details);
    }
}

ScopedStage::ScopedStage(ScopedStage&& other) noexcept
    : reporter_(other.reporter_),
      label_(std::move(other.label_)),
      startedAt_(other.startedAt_),
      active_(other.active_) {
    other.reporter_ = nullptr;
    other.active_ = false;
}

ScopedStage& ScopedStage::operator=(ScopedStage&& other) noexcept {
    if (this != &other) {
        if (active_ && reporter_) {
            reporter_->endStage(label_, startedAt_);
        }
        reporter_ = other.reporter_;
        label_ = std::move(other.label_);
        startedAt_ = other.startedAt_;
        active_ = other.active_;
        other.reporter_ = nullptr;
        other.active_ = false;
    }
    return *this;
}

ScopedStage::~ScopedStage() {
    if (active_ && reporter_) {
        reporter_->endStage(label_, startedAt_);
    }
}

DebugReporter::DebugReporter(std::string component, std::ostream& output)
    : component_(std::move(component)),
      startedAt_(Clock::now()),
      output_(&output) {
    event("debug_start");
}

DebugReporter::DebugReporter(std::string component)
    : DebugReporter(std::move(component), std::cout) {}

void DebugReporter::event(const std::string& label, const std::string& details) const {
    write(label, details);
}

ScopedStage DebugReporter::scope(const std::string& label, const std::string& details) {
    return ScopedStage(this, label, details);
}

void DebugReporter::beginStage(const std::string& label, const std::string& details) const {
    write(label + ".begin", details);
}

void DebugReporter::endStage(const std::string& label, Clock::time_point startedAt) const {
    write(label + ".end",
          "duration_ms=" + formatMilliseconds(elapsedMilliseconds(startedAt)));
}

double DebugReporter::elapsedMilliseconds(Clock::time_point at) const {
    return std::chrono::duration<double, std::milli>(Clock::now() - at).count();
}

std::string DebugReporter::formatMilliseconds(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

void DebugReporter::write(const std::string& label, const std::string& details) const {
    std::lock_guard<std::mutex> lock(mutex_);
    *output_ << "[debug][" << component_ << "][+"
             << formatMilliseconds(elapsedMilliseconds(startedAt_))
             << " ms] " << label;
    if (!details.empty()) {
        *output_ << " " << details;
    }
    *output_ << std::endl;
}

}  // namespace Common::debug
