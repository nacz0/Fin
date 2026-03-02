#pragma once
#include <string>
#include <vector>

struct AppConfig {
    std::string lastDirectory;
    std::string language = "pl";
    float zoom = 1.0f;
    int windowWidth = 1280;
    int windowHeight = 720;

    std::vector<std::string> openFiles;
    int activeTabIndex = -1;
    int theme = 0; // 0: Dark, 1: Light, 2: RetroBlue, 3: ClassicIDE
    
    bool autocompleteEnabled = true;
    bool clangBuildEnabled = true;
    bool autoClosingBrackets = true;
    bool smartIndentEnabled = true;
    bool minimapEnabled = true;
    bool showSettingsWindow = false;
};
