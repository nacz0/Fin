#include "Utils.h" // Dołączamy nasz nagłówek
#include <fstream>
#include <sstream>
#include <array>
#include <cstdio>
#include <memory>
#include <iostream>

const char* CONFIG_FILE = "fin.ini";

void SaveConfig(const AppConfig& config) {
    std::ofstream out(CONFIG_FILE);
    if (out.is_open()) {
        out << "dir=" << config.lastDirectory << "\n";
        out << "zoom=" << config.zoom << "\n";
        out << "width=" << config.windowWidth << "\n";
        out << "height=" << config.windowHeight << "\n";
        out << "activeTab=" << config.activeTabIndex << "\n";
        out << "theme=" << config.theme << "\n";
        
        // Zapisujemy każdą ścieżkę w nowej linii
        for (const auto& path : config.openFiles) {
            if (!path.empty()) {
                out << "file=" << path << "\n";
            }
        }
        out.close();
    }
}

AppConfig LoadConfig() {
    AppConfig config;
    config.lastDirectory = ".";
    config.activeTabIndex = -1;

    std::ifstream in(CONFIG_FILE);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            size_t sep = line.find('=');
            if (sep != std::string::npos) {
                std::string key = line.substr(0, sep);
                std::string value = line.substr(sep + 1);

                if (key == "dir") config.lastDirectory = value;
                else if (key == "zoom") config.zoom = std::stof(value);
                else if (key == "width") config.windowWidth = std::stoi(value);
                else if (key == "height") config.windowHeight = std::stoi(value);
                else if (key == "activeTab") config.activeTabIndex = std::stoi(value);
                else if (key == "theme") config.theme = std::stoi(value);
                else if (key == "file") config.openFiles.push_back(value); // Dodajemy ścieżkę do listy
            }
        }
        in.close();
    }
    return config;
}

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