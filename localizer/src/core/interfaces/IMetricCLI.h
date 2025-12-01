#pragma once
#include <argparse/argparse.hpp>
#include <memory>
#include <string>

namespace Pipeline {

class IMetricCLI {
public:
    virtual ~IMetricCLI() = default;

    virtual std::string getSubcommandName() const = 0;
    virtual std::string getDescription() const = 0;

    virtual void configureArguments(argparse::ArgumentParser& parser) = 0;
    virtual int execute(const argparse::ArgumentParser& parser, const std::string& fullCommand) = 0;
};

using MetricCLIPtr = std::shared_ptr<IMetricCLI>;

} // namespace Pipeline

