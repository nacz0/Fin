#include "EditorLogic.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <cstdio>

// --- PAMIĘĆ STANU ---
static bool g_wasAutoClosed = false;
static TextEditor::Coordinates g_lastPos = { 0, 0 };
static bool g_shouldDeleteClosing = false;
static int g_deleteAtLine = -1;
static int g_deleteAtColumn = -1;

void HandlePreRenderLogic(TextEditor& editor) {
    ImGuiIO& io = ImGui::GetIO();
    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();

    if (pos.mLine < 0 || pos.mLine >= (int)lines.size()) return;

    // 1. MONITOROWANIE RUCHU
    if (g_wasAutoClosed) {
        if (pos.mLine != g_lastPos.mLine || pos.mColumn != g_lastPos.mColumn) {
            g_wasAutoClosed = false;
        }
    }

    // 2. BACKSPACE CHECK
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !editor.HasSelection()) {
        if (g_wasAutoClosed) {
            const std::string& line = lines[pos.mLine];
            if (pos.mColumn > 0 && pos.mColumn < (int)line.size()) {
                char charLeft = line[pos.mColumn - 1];
                char charRight = line[pos.mColumn];

                bool isPair = (charLeft == '(' && charRight == ')') ||
                              (charLeft == '[' && charRight == ']') ||
                              (charLeft == '"' && charRight == '"') ||
                              (charLeft == '\'' && charRight == '\'');

                if (isPair) {
                    g_shouldDeleteClosing = true;
                    g_deleteAtLine = pos.mLine;
                    g_deleteAtColumn = pos.mColumn - 1;
                }
            }
        }
        g_wasAutoClosed = false;
    }

    // 3. AUTO-DOMYKANIE
    if (io.InputQueueCharacters.Size > 0) {
        for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
            unsigned int c = io.InputQueueCharacters[n];
            char closingChar = 0;
            if (c == '(') closingChar = ')';
            else if (c == '[') closingChar = ']';
            else if (c == '"') closingChar = '"';
            else if (c == '\'') closingChar = '\'';

            if (closingChar != 0) {
                editor.InsertText(std::string(1, closingChar));
                auto p = editor.GetCursorPosition();
                if (p.mColumn > 0) {
                    editor.SetCursorPosition(TextEditor::Coordinates(p.mLine, p.mColumn - 1));
                    g_wasAutoClosed = true;
                    g_lastPos.mLine = p.mLine;
                    g_lastPos.mColumn = p.mColumn;
                }
            } else {
                g_wasAutoClosed = false;
            }
        }
    }
}

void HandlePostRenderLogic(TextEditor& editor) {
    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();

    // 1. USUWAMY ZAMYKAJĄCY ZNAK (Metoda Delete())
    if (g_shouldDeleteClosing) {
        g_shouldDeleteClosing = false;
        if (pos.mLine == g_deleteAtLine && pos.mColumn == g_deleteAtColumn) {
            if (pos.mLine >= 0 && pos.mLine < (int)lines.size()) {
                const std::string& line = lines[pos.mLine];
                if (pos.mColumn < (int)line.size()) {
                    char c = line[pos.mColumn];
                    if (c == ')' || c == ']' || c == '"' || c == '\'') {
                        // Jawne użycie TextEditor::Coordinates naprawia błąd C2664
                        editor.SetSelection(
                            TextEditor::Coordinates(pos.mLine, pos.mColumn), 
                            TextEditor::Coordinates(pos.mLine, pos.mColumn + 1)
                        );
                        editor.Delete();
                    }
                }
            }
        }
    }

    // 2. SMART ENTER DLA KLAMR
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (pos.mLine <= 0 || pos.mLine >= (int)lines.size()) return;
        const std::string& prevLine = lines[pos.mLine - 1];
        size_t lastChar = prevLine.find_last_not_of(" \t\r\n");

        if (lastChar != std::string::npos && prevLine[lastChar] == '{') {
            std::string indent = "";
            for (char ch : prevLine) { if (ch == ' ' || ch == '\t') indent += ch; else break; }
            
            const std::string& currentLine = lines[pos.mLine]; // Naprawiony błąd C2065
            size_t firstChar = currentLine.find_first_not_of(" \t\r\n");

            if (firstChar == std::string::npos) {
                editor.SetSelection(
                    TextEditor::Coordinates(pos.mLine, 0), 
                    TextEditor::Coordinates(pos.mLine, (int)currentLine.size())
                );
                editor.InsertText("");
                editor.InsertText(indent + "    \n" + indent + "}");
                
                auto newPos = editor.GetCursorPosition();
                if (newPos.mLine > 0) {
                    editor.SetCursorPosition(TextEditor::Coordinates(newPos.mLine - 1, (int)indent.size() + 4));
                }
            }
        }
    }
}