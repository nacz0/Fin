#include "AutocompleteLogic.h"
#include "imgui.h"
#include <iostream>
#include <mutex>

void HandleAutocompleteLogic(EditorTab& tab, LSPClient& lsp, const AppConfig& config) {
    if (!config.autocompleteEnabled) {
        tab.acState->show = false;
        tab.acState->items.clear();
        tab.editor.SetHandleKeyboardInputs(true);
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    auto& editor = tab.editor;
    auto pos = editor.GetCursorPosition();

    bool ctrlSpace = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Space);
    
    auto& lines = editor.GetTextLines();
    if (pos.mLine >= 0 && pos.mLine < (int)lines.size()) {
        const std::string& line = lines[pos.mLine];
        bool shouldTrigger = false;
        
        if (ctrlSpace) {
            shouldTrigger = true;
        } else if (pos.mColumn > 0 && pos.mColumn <= (int)line.size()) {
            char c = line[pos.mColumn - 1];
            
            if (c == '.') {
                shouldTrigger = true;
            }
            else if (c == '>' && pos.mColumn >= 2 && line[pos.mColumn - 2] == '-') {
                shouldTrigger = true;
            }
            else if (c == ':' && pos.mColumn >= 2 && line[pos.mColumn - 2] == ':') {
                shouldTrigger = true;
            }
        }
            
        if (shouldTrigger) {
            static int lastTriggerLine = -1;
            static int lastTriggerCol = -1;
            if (ctrlSpace || pos.mLine != lastTriggerLine || pos.mColumn != lastTriggerCol) {
                tab.acState->requested = true;
                lastTriggerLine = pos.mLine;
                lastTriggerCol = pos.mColumn;
                std::cout << "[Editor] Triggering autocomplete at " << pos.mLine << ":" << pos.mColumn << (ctrlSpace ? " (Ctrl+Space)" : "") << std::endl;
            }
        }
    }

    if (tab.acState->requested) {
        tab.acState->requested = false;
        tab.acState->coord = pos;
        
        lsp.DidChange(tab.path, editor.GetText());
        
        auto state = tab.acState;
        lsp.RequestCompletion(tab.path, pos.mLine, pos.mColumn, [state](const std::vector<LSPCompletionItem>& items) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->pendingResults.clear();
            for (auto& i : items) {
                state->pendingResults.push_back({ i.label, i.detail, i.insertText });
            }
            state->hasNewResults = true;
        });
    }

    {
        std::lock_guard<std::mutex> lock(tab.acState->mutex);
        if (tab.acState->hasNewResults) {
            std::cout << "[Editor] Transferring " << tab.acState->pendingResults.size() << " results from LSP" << std::endl;
            tab.acState->items = tab.acState->pendingResults;
            tab.acState->hasNewResults = false;
            if (!tab.acState->items.empty()) {
                tab.acState->show = true;
                tab.acState->selectedIndex = 0;
                std::cout << "[Editor] Popup should now be visible with " << tab.acState->items.size() << " items" << std::endl;
            } else {
                tab.acState->show = false;
                std::cout << "[Editor] No items to show, hiding popup" << std::endl;
            }
        }
    }

    if (tab.acState->show && !tab.acState->items.empty()) {
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            tab.acState->show = false;
            editor.SetHandleKeyboardInputs(true);
            return;
        }

        if (pos.mLine != tab.acState->coord.mLine || pos.mColumn != tab.acState->coord.mColumn) {
            tab.acState->show = false;
            editor.SetHandleKeyboardInputs(true);
            return;
        }

        editor.SetHandleKeyboardInputs(false);

        int itemCount = (int)tab.acState->items.size();
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            tab.acState->selectedIndex--;
            if (tab.acState->selectedIndex < 0) tab.acState->selectedIndex = itemCount - 1;
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            tab.acState->selectedIndex++;
            if (tab.acState->selectedIndex >= itemCount) tab.acState->selectedIndex = 0;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            if (tab.acState->selectedIndex >= 0 && tab.acState->selectedIndex < itemCount) {
                auto& item = tab.acState->items[tab.acState->selectedIndex];
                editor.SetHandleKeyboardInputs(true);
                editor.InsertText(item.insertText);
                editor.SetHandleKeyboardInputs(false);
                
                if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    tab.acState->justConsumedEnter = true;
                }
            }
            tab.acState->show = false;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            tab.acState->show = false;
        }
        
        if (ImGui::GetIO().InputQueueCharacters.Size > 0) {
             tab.acState->show = false;
        }
    } else {
        editor.SetHandleKeyboardInputs(true);
    }
}
