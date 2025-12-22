#include "MenuBar.h"
#include "../Editor/ThemeManager.h"
#include "imgui.h"

void ShowMainMenuBar(LSPClient& lsp, AppConfig& config, std::vector<std::unique_ptr<EditorTab>>& tabs, int activeTab, fs::path& currentPath, int& nextTabToFocus, bool& actionNew, bool& actionOpen, bool& actionSave, bool& actionCloseTab, bool& actionSearch, bool& actionBuild) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Plik")) {
            if (ImGui::MenuItem("Nowy", "Ctrl+N")) actionNew = true;
            if (ImGui::MenuItem("Otworz", "Ctrl+O")) actionOpen = true;
            if (ImGui::MenuItem("Zapisz", "Ctrl+S")) actionSave = true;
            if (ImGui::MenuItem("Zamknij Karte", "Ctrl+W")) actionCloseTab = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edycja")) {
            if (ImGui::MenuItem("Szukaj", "Ctrl+F")) actionSearch = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Buduj")) {
            if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) actionBuild = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Widok")) {
            if (ImGui::BeginMenu("Motyw")) {
                if (ImGui::MenuItem("Ciemny", nullptr, config.theme == 0)) {
                    config.theme = 0;
                    ThemeManager::ApplyTheme(0, tabs);
                }
                if (ImGui::MenuItem("Jasny", nullptr, config.theme == 1)) {
                    config.theme = 1;
                    ThemeManager::ApplyTheme(1, tabs);
                }
                if (ImGui::MenuItem("Retro (Niebieski)", nullptr, config.theme == 2)) {
                    config.theme = 2;
                    ThemeManager::ApplyTheme(2, tabs);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Ustawienia", nullptr, config.showSettingsWindow)) {
                config.showSettingsWindow = !config.showSettingsWindow;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
