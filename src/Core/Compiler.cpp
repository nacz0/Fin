#include "Compiler.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

std::vector<ParsedError> ParseCompilerOutput(const std::string& output) {
    std::vector<ParsedError> errors;
    std::stringstream ss(output);
    std::string line;
    const std::regex fileLineColRegex(R"(^(.*):(\d+):(\d+):\s+(.*)$)");
    const std::regex genericErrorRegex(R"(^(.+):\s*(fatal error|error):\s*(.*)$)", std::regex::icase);
    std::smatch matches;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (std::regex_search(line, matches, fileLineColRegex)) {
            if (matches.size() >= 5) {
                std::string message = matches[4].str();
                std::string lowered = message;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });

                if (lowered.find("error") == std::string::npos) {
                    continue;
                }

                ParsedError err;
                err.fullMessage = line;
                err.filename = matches[1].str();
                err.line = std::stoi(matches[2].str());
                err.col = std::stoi(matches[3].str());
                err.message = message;
                err.isError = true;
                errors.push_back(err);
            }
            continue;
        }

        if (std::regex_search(line, matches, genericErrorRegex) && matches.size() >= 4) {
            ParsedError err;
            err.fullMessage = line;
            err.filename = matches[1].str();
            err.message = matches[2].str() + ": " + matches[3].str();
            err.isError = true;
            errors.push_back(err);
        }
    }

    return errors;
}
