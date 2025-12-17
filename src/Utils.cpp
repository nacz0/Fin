#include "Utils.h" // Dołączamy nasz nagłówek
#include <fstream>
#include <sstream>
#include <array>
#include <cstdio>
#include <memory>
#include <iostream>

void SaveFile(const std::string& filename, const std::string& text) {
    std::ofstream out(filename);
    out << text;
    out.close();
}

std::string OpenFile(const std::string& filename) {
    std::ifstream in(filename);
    if (in) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return "";
}

std::string ExecCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // _popen to specyficzne dla Windows (na Linux byłoby popen)
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) return "popen() failed!";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}