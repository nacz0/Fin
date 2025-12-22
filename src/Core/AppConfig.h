#pragma once
#include <string>
#include <vector>

struct AppConfig {
    std::string lastDirectory;
    float zoom = 1.0f;
    int windowWidth = 1280;
    int windowHeight = 720;

    std::vector<std::string> openFiles;
    int activeTabIndex = -1;
    int theme = 0; // 0: Dark, 1: Light, 2: RetroBlue
    
    bool autocompleteEnabled = true;
    bool autoClosingBrackets = true;
    bool smartIndentEnabled = true;
    bool showSettingsWindow = false;
};
