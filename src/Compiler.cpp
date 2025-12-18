#include "Compiler.h"
#include <sstream>
#include <regex>

std::vector<ParsedError> ParseCompilerOutput(const std::string& output) {
    std::vector<ParsedError> errors;
    std::stringstream ss(output);
    std::string line;
    // Regex łapiący format: plik:linia:kolumna: wiadomosc
    std::regex errorRegex(R"(^(.*):(\d+):(\d+):\s+(.*)$)"); 
    std::smatch matches;

    while (std::getline(ss, line)) {
        // Fix dla Windowsa (usuwanie \r z końca linii)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        ParsedError err;
        err.fullMessage = line; 

        if (std::regex_search(line, matches, errorRegex)) {
            if (matches.size() >= 5) {
                err.filename = matches[1].str();
                err.line = std::stoi(matches[2].str());
                err.col = std::stoi(matches[3].str());
                err.message = matches[4].str(); 
                
                // Wykrywanie ostrzeżeń
                if (err.message.find("warning") != std::string::npos || 
                    err.message.find("note") != std::string::npos) {
                    err.isError = false;
                }
            }
        }
        errors.push_back(err);
    }
    return errors;
}