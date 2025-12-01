#pragma once
#include <argparse/argparse.hpp>
#include <string>

// Command execution functions
int executeCentiloidCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand);
int executeCenTauRCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand);
int executeCenTauRzCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand);
int executeFillStatesCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand);
int executeSUVrCommand(const argparse::ArgumentParser& parser, const std::string& fullCommand);
int executeDecoupleCommand(const argparse::ArgumentParser& parser);

