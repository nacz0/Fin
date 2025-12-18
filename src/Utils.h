#pragma once // To sprawia, że plik dołączy się tylko raz (zamiast się dublować)
#include <string>
#include <vector>

struct AppConfig {
    std::string lastDirectory; // Ostatnio otwarty folder
    float zoom = 1.0f;         // Poziom powiększenia
    int windowWidth = 1280;    // Szerokość okna
    int windowHeight = 720;    // Wysokość okna

    std::vector<std::string> openFiles; // Lista ścieżek do otwartych plików
    int activeTabIndex = -1; // Indeks aktywnej karty
};

void SaveConfig(const AppConfig& config);
AppConfig LoadConfig();

// Deklaracje funkcji (obietnica, że one istnieją)
void SaveFile(const std::string& filename, const std::string& text);
std::string OpenFile(const std::string& filename);
std::string ExecCommand(const char* cmd);