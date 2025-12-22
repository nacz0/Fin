#include "TerminalPanel.h"
#include "imgui.h"

void ShowTerminal(Terminal& terminal) {
    if (!ImGui::Begin("Terminal")) {
        ImGui::End();
        return;
    }
    
    static std::string history, prompt;
    static std::string totalOutput;
    
    std::string newOutput = terminal.GetOutput();
    if (!newOutput.empty()) {
        totalOutput += newOutput;
        if (totalOutput.size() > 100000) {
            totalOutput = totalOutput.substr(totalOutput.size() - 50000);
        }
        
        size_t lastNewline = totalOutput.find_last_of('\n');
        if (lastNewline != std::string::npos) {
            history = totalOutput.substr(0, lastNewline + 1);
            prompt = totalOutput.substr(lastNewline + 1);
        } else {
            history.clear();
            prompt = totalOutput;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    
    ImGui::BeginChild("TerminalScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (!history.empty()) {
        ImGui::TextUnformatted(history.c_str());
    }

    ImGui::TextUnformatted(prompt.c_str());
    ImGui::SameLine(0, 0);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    static char inputBuf[512] = "";
    bool reclaim_focus = false;
    
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##TerminalInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        terminal.SendInput(inputBuf);
        inputBuf[0] = '\0';
        reclaim_focus = true;
    }
    
    if (reclaim_focus || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive())) {
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SetScrollHereY(1.0f);
        
    ImGui::EndChild();
    
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(1);

    ImGui::End();
}
