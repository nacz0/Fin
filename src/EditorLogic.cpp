#include "EditorLogic.h"
#include "imgui.h"
#include <string>

void HandlePreRenderLogic(TextEditor& editor) {
    ImGuiIO& io = ImGui::GetIO();
    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();

    if (pos.mLine >= (int)lines.size()) return;

    // 1. INTELIGENTNY BACKSPACE (Usuwanie pustych par)
    // Działa tylko wtedy, gdy nie ma zaznaczenia
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !editor.HasSelection()) {
        if (pos.mLine < (int)lines.size()) {
            const std::string& line = lines[pos.mLine];
            // Sprawdzamy sąsiadów: znak przed kursorem i znak na kursorze
            if (pos.mColumn > 0 && pos.mColumn < (int)line.size()) {
                char charLeft = line[pos.mColumn - 1];
                char charRight = line[pos.mColumn];
                
                // Jeśli to para nawiasów
                if ((charLeft == '(' && charRight == ')') ||
                    (charLeft == '[' && charRight == ']') ||
                    (charLeft == '{' && charRight == '}') ||
                    (charLeft == '"' && charRight == '"') ||
                    (charLeft == '\'' && charRight == '\'')) 
                {
                    // Usuwamy prawy znak (zamykający)
                    // Lewy znak (otwierający) usunie sam edytor chwilę później (bo wcisnąłeś Backspace)
                    editor.SetSelection(pos, { pos.mLine, pos.mColumn + 1 });
                    editor.InsertText("");
                }
            }
        }
    }

    // 2. AUTO-DOMYKANIE (Wpisywanie par)
    for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
        unsigned int c = io.InputQueueCharacters[n];
        char closingChar = 0;
        
        if (c == '(') closingChar = ')';
        else if (c == '[') closingChar = ']';
        else if (c == '"') closingChar = '"';
        else if (c == '\'') closingChar = '\'';
        // Klamry {} są obsługiwane przez Enter w PostRender, więc tu ich nie dodajemy

        if (closingChar != 0) {
            editor.InsertText(std::string(1, closingChar));
            auto p = editor.GetCursorPosition();
            editor.SetCursorPosition({ p.mLine, p.mColumn - 1 });
        }
    }
}

void HandlePostRenderLogic(TextEditor& editor) {
    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();

    if (pos.mLine >= (int)lines.size()) return;

    // 3. INTELIGENTNY ENTER (Formatowanie klamr {})
    // Wykonuje się PO tym, jak edytor wstawił już nową linię
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        // Musimy być w linii > 0, bo sprawdzamy linię wyżej (tę z klamrą)
        if (pos.mLine > 0) {
            std::string prevLine = lines[pos.mLine - 1];
            size_t lastChar = prevLine.find_last_not_of(" \t\r\n");
            
            // Jeśli poprzednia linia kończy się na '{'
            if (lastChar != std::string::npos && prevLine[lastChar] == '{') {
                
                // Pobieramy wcięcie z góry
                std::string indent = "";
                for (char c : prevLine) {
                    if (c == ' ' || c == '\t') indent += c;
                    else break;
                }

                // Sprawdzamy czy obecna linia jest pusta (bezpiecznik)
                std::string currentLine = lines[pos.mLine];
                bool isEmptyOrIndent = true;
                for (char c : currentLine) {
                    if (c != ' ' && c != '\t') { isEmptyOrIndent = false; break; }
                }

                if (isEmptyOrIndent) {
                    // Czyścimy obecną linię (usuwamy auto-indent edytora)
                    editor.SetSelection({ pos.mLine, 0 }, { pos.mLine, (int)currentLine.size() });
                    editor.InsertText(""); 
                    
                    // Wstawiamy: wcięcie+4spacje + nowa linia + wcięcie + }
                    std::string block = indent + "    \n" + indent + "}";
                    editor.InsertText(block);
                    
                    // Ustawiamy kursor w środku (na końcu 4 spacji)
                    editor.SetCursorPosition({ pos.mLine, (int)indent.size() + 4 });
                }
            }
        }
    }
}