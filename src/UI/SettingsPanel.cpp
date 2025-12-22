#include "SettingsPanel.h"
#include "imgui.h"
#include <filesystem>

namespace fs = std::filesystem;

void ShowSettings(AppConfig& config, float& textScale, LSPClient& lsp, std::vector<std::unique_ptr<EditorTab>>& tabs) {
    if (!config.showSettingsWindow) return;

    if (ImGui::Begin("Ustawienia", &config.showSettingsWindow)) {
        ImGui::Text("Edytor");
        ImGui::Separator();
        
        bool prevAutocomplete = config.autocompleteEnabled;
        ImGui::Checkbox("Autouzupełnianie (LSP)", &config.autocompleteEnabled);
        
        if (prevAutocomplete != config.autocompleteEnabled) {
            if (config.autocompleteEnabled) {
                if (!lsp.IsRunning()) {
                    if (lsp.Start()) {
                        lsp.Initialize(fs::current_path().string());
                        for (auto& t : tabs) {
                            if (!t->path.empty()) {
                                lsp.DidOpen(t->path, t->editor.GetText());
                            }
                        }
                    }
                }
            } else {
                lsp.Stop();
                for (auto& t : tabs) {
                    t->lspDiagnostics.clear();
                    t->editor.SetErrorMarkers(TextEditor::ErrorMarkers());
                }
            }
        }
        
        ImGui::Checkbox("Auto-domykanie nawiasów/cudzysłowów", &config.autoClosingBrackets);
        ImGui::Checkbox("Inteligentne wcięcia (Smart Indent)", &config.smartIndentEnabled);
        
        ImGui::Separator();
        ImGui::Text("Wygląd");
        if (ImGui::SliderFloat("Zoom", &textScale, 0.5f, 3.0f, "%.1fx")) {
            config.zoom = textScale;
        }
    }
    ImGui::End();
}
