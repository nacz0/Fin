#include "ConfigManager.h"
#include <fstream>
#include <sstream>

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
        out << "ac=" << (config.autocompleteEnabled ? "1" : "0") << "\n";
        out << "brackets=" << (config.autoClosingBrackets ? "1" : "0") << "\n";
        out << "indent=" << (config.smartIndentEnabled ? "1" : "0") << "\n";
        
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
                else if (key == "ac") config.autocompleteEnabled = (value == "1");
                else if (key == "brackets") config.autoClosingBrackets = (value == "1");
                else if (key == "indent") config.smartIndentEnabled = (value == "1");
                else if (key == "file") config.openFiles.push_back(value);
            }
        }
        in.close();
    }
    return config;
}
