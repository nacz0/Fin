#include "AutocompletePopup.h"
#include "imgui.h"
#include <iostream>

void RenderAutocompletePopup(EditorTab& tab, float textScale, LSPClient& lsp) {
    if (!lsp.IsRunning() || !tab.acState->show || tab.acState->items.empty()) {
        tab.acState->show = false;
        return;
    }
    
    std::cout << "[UI] Rendering autocomplete popup with " << tab.acState->items.size() << " items" << std::endl;
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.y += ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing);
    
    if (ImGui::Begin("Autocomplete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        for (int i = 0; i < (int)tab.acState->items.size(); i++) {
            bool selected = (i == tab.acState->selectedIndex);
            if (ImGui::Selectable(tab.acState->items[i].label.c_str(), selected)) {
                tab.editor.InsertText(tab.acState->items[i].insertText);
                tab.acState->show = false;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::End();
}
