#include "ExplorerPanel.h"
#include "../Core/FileManager.h"
#include "../Editor/ThemeManager.h"
#include <algorithm>
#include <chrono>

void ShowExplorer(LSPClient& lsp, AppConfig& config, fs::path& currentPath, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus, float textScale, ImGuiIO& io) {
    static fs::path lastPath;
    static std::vector<FileEntry> cachedEntries;
    static auto lastRefresh = std::chrono::steady_clock::now();

    bool forceRefresh = false;
    ImGui::Begin("Eksplorator");
    if (ImGui::Button(".. (W gore)") || (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
         fs::path absolutePath = fs::absolute(currentPath);
         if (absolutePath.has_parent_path()) {
             fs::path parent = absolutePath.parent_path();
             if (parent != absolutePath) {
                 currentPath = parent;
                 forceRefresh = true;
             }
         }
    }
    
    auto now = std::chrono::steady_clock::now();
    if (currentPath != lastPath || forceRefresh || std::chrono::duration_cast<std::chrono::seconds>(now - lastRefresh).count() >= 1) {
        cachedEntries.clear();
        try {
            for (auto& e : fs::directory_iterator(currentPath)) {
                cachedEntries.push_back({
                    e.path().filename().string(),
                    e.path().string(),
                    e.path().extension().string(),
                    e.is_directory()
                });
            }
            std::sort(cachedEntries.begin(), cachedEntries.end(), [](const FileEntry& a, const FileEntry& b) {
                if (a.isDirectory != b.isDirectory) return a.isDirectory;
                return a.name < b.name;
            });
        } catch(...) {}
        lastPath = currentPath;
        lastRefresh = now;
    }

    ImGui::Separator();
    
    for (const auto& entry : cachedEntries) {
        if (entry.isDirectory) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
            if (ImGui::Selectable(("[DIR] " + entry.name).c_str())) {
                currentPath = entry.path;
            }
            ImGui::PopStyleColor();
        } else {
            if (entry.extension == ".exe" || entry.extension == ".bin") {
                ImGui::TextDisabled("  %s [BIN]", entry.name.c_str());
            } else if (ImGui::Selectable(("  " + entry.name).c_str())) {
                std::string p = entry.path;
                bool open = false;
                for(int i=0; i<(int)tabs.size(); i++) {
                    if(tabs[i]->path == p) {
                        nextTabToFocus = i;
                        open = true;
                        break;
                    }
                }
                if(!open) {
                    auto nt = std::make_unique<EditorTab>(); 
                    nt->configRef = &config;
                    nt->name = entry.name; nt->path = p; nt->editor.SetText(OpenFile(p));
                    ThemeManager::ApplyTheme(config.theme, *nt);
                    if (lsp.IsRunning()) lsp.DidOpen(p, nt->editor.GetText());
                    tabs.push_back(std::move(nt)); 
                    nextTabToFocus = (int)tabs.size() - 1;
                }
            }
        }
    }
    ImGui::End();
}
