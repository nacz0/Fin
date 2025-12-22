#include "ConsolePanel.h"
#include "imgui.h"
#include <filesystem>

namespace fs = std::filesystem;

void ShowConsole(bool isCompiling, const std::string& compilationOutput, std::vector<ParsedError>& errorList, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus) {
    ImGui::Begin("Konsola Wyjscia");
    if (isCompiling) ImGui::TextColored(ImVec4(1, 1, 0, 1), "KOMPILACJA...");
    if (errorList.empty()) ImGui::TextWrapped("%s", compilationOutput.c_str());
    else {
        for (auto& err : errorList) {
            if (err.line == 0) ImGui::TextWrapped("%s", err.fullMessage.c_str());
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, err.isError ? ImVec4(1,0.4f,0.4f,1) : ImVec4(1,1,0.4f,1));
                if (ImGui::Selectable((err.filename + ":" + std::to_string(err.line) + " " + err.message).c_str())) {
                    for(int i=0; i<(int)tabs.size(); i++) {
                        if(fs::path(tabs[i]->path).filename() == fs::path(err.filename).filename()) {
                            nextTabToFocus = i;
                            if (err.line > 0) {
                                tabs[i]->editor.SetCursorPosition(TextEditor::Coordinates(err.line-1, 0));
                            }
                            break;
                        }
                    }
                }
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::End();
}
