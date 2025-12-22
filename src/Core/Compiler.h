#pragma once
#include <string>
#include <vector>

struct ParsedError {
    std::string fullMessage;
    std::string filename;
    int line = 0;
    int col = 0;
    std::string message;
    bool isError = true;
};

std::vector<ParsedError> ParseCompilerOutput(const std::string& output);