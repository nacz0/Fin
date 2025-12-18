#include "EditorLogic.h"
#include "imgui.h" // Potrzebne do IsKeyPressed
#include <string>
#include <vector>
#include <sstream> // Potrzebne do formatowania logów

// --- PAMIĘĆ STANU (Twoje zmienne) ---
static bool g_wasAutoClosed = false;
static TextEditor::Coordinates g_lastPos = { 0, 0 };
static bool g_shouldDeleteClosing = false;
static int g_deleteAtLine = -1;
static int g_deleteAtColumn = -1;

// --- LOGIKA ISTNIEJĄCA (Bez zmian) ---
void HandlePreRenderLogic(TextEditor& editor) {
    ImGuiIO& io = ImGui::GetIO();
    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();

    if (pos.mLine < 0 || pos.mLine >= (int)lines.size()) return;

    if (g_wasAutoClosed) {
        if (pos.mLine != g_lastPos.mLine || pos.mColumn != g_lastPos.mColumn) {
            g_wasAutoClosed = false;
        }
    }

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

    if (g_shouldDeleteClosing) {
        g_shouldDeleteClosing = false;
        if (pos.mLine == g_deleteAtLine && pos.mColumn == g_deleteAtColumn) {
            if (pos.mLine >= 0 && pos.mLine < (int)lines.size()) {
                const std::string& line = lines[pos.mLine];
                if (pos.mColumn < (int)line.size()) {
                    char c = line[pos.mColumn];
                    if (c == ')' || c == ']' || c == '"' || c == '\'') {
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

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (pos.mLine <= 0 || pos.mLine >= (int)lines.size()) return;
        const std::string& prevLine = lines[pos.mLine - 1];
        size_t lastChar = prevLine.find_last_not_of(" \t\r\n");

        if (lastChar != std::string::npos && prevLine[lastChar] == '{') {
            std::string indent = "";
            for (char ch : prevLine) { if (ch == ' ' || ch == '\t') indent += ch; else break; }
            
            const std::string& currentLine = lines[pos.mLine];
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

// --- NOWA, NIEZAWODNA LOGIKA WYSZUKIWANIA ---


void FindNext(TextEditor& editor, const std::string& query) {
    if (query.empty()) return;

    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();
    int totalLines = (int)lines.size();

    // KROK 1: Sprawdź bieżącą linię od kursora w prawo
    if (pos.mLine < totalLines) {
        const std::string& currentLine = lines[pos.mLine];
        
        // Zaczynamy od pos.mColumn. Jeśli już coś jest wybrane, 
        // automatycznie szukamy dalej (bo kursor będzie na końcu poprzedniego znaleziska).
        size_t found = currentLine.find(query, pos.mColumn);
        
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = {pos.mLine, (int)found};
            TextEditor::Coordinates sEnd = {pos.mLine, (int)(found + query.length())};
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd); // Przesuń kursor na koniec, by następne "Szukaj" szukało DALEJ
            return;
        }
    }

    // KROK 2: Sprawdź wszystkie linie PONIŻEJ
    for (int i = pos.mLine + 1; i < totalLines; ++i) {
        size_t found = lines[i].find(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd);
            return;
        }
    }

    // KROK 3: Zawijanie - sprawdź od początku pliku (linia 0) do kursora
    for (int i = 0; i <= pos.mLine; ++i) {
        size_t found = std::string::npos;
        
        if (i == pos.mLine) {
            // W linii kursora sprawdź tylko to, co jest przed kursorem
            std::string sub = lines[i].substr(0, pos.mColumn);
            found = sub.find(query);
        } else {
            found = lines[i].find(query);
        }

        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd);
            return;
        }
    }
}

void FindPrev(TextEditor& editor, const std::string& query) {
    if (query.empty()) return;

    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();
    int totalLines = (int)lines.size();

    // ETAP 1: Szukaj w bieżącej linii PRZED kursorem
    if (pos.mLine < totalLines && pos.mColumn > 0) {
        // rfind szuka od prawej strony, ale nie dalej niż wskazany indeks
        size_t searchLimit = pos.mColumn; 
        
        // Jeśli jesteśmy na początku słowa/linii, musimy się cofnąć o 1, żeby nie znaleźć tego samego
        if (searchLimit > 0) searchLimit--; 

        size_t found = lines[pos.mLine].rfind(query, searchLimit);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { pos.mLine, (int)found };
            TextEditor::Coordinates sEnd = { pos.mLine, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart); // Przy szukaniu W TYŁ kursor dajemy na początek
            return;
        }
    }

    // ETAP 2: Szukaj w liniach POWYŻEJ bieżącej
    for (int i = pos.mLine - 1; i >= 0; --i) {
        size_t found = lines[i].rfind(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
            return;
        }
    }

    // ETAP 3: (Pętla) Szukaj od SAMEGO DOŁU pliku do linii pod kursorem
    for (int i = totalLines - 1; i > pos.mLine; --i) {
        size_t found = lines[i].rfind(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
            return;
        }
    }

    // ETAP 4: (Pętla) Szukaj w bieżącej linii od KOŃCA LINI do kursora
    if (pos.mLine < totalLines) {
        // rfind bez drugiego argumentu szuka od samego końca stringa
        size_t found = lines[pos.mLine].rfind(query);
        // Ale interesuje nas tylko, jeśli jest ZA kursorem (bo to pętla "od tyłu")
        if (found != std::string::npos && found >= (size_t)pos.mColumn) {
            TextEditor::Coordinates sStart = { pos.mLine, (int)found };
            TextEditor::Coordinates sEnd = { pos.mLine, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
        }
    }
}

void UpdateSearchInfo(TextEditor& editor, const std::string& query, int& outCount, int& outIndex) {
    outCount = 0;
    outIndex = 0;
    if (query.empty()) return;

    auto& lines = editor.GetTextLines();
    auto cursorPos = editor.GetCursorPosition();

    for (int i = 0; i < (int)lines.size(); ++i) {
        size_t found = lines[i].find(query);
        while (found != std::string::npos) {
            outCount++;

            // Sprawdzamy, czy kursor znajduje się w obrębie tego znaleziska
            if (i == cursorPos.mLine) {
                int start = (int)found;
                int end = (int)(found + query.length());
                if (cursorPos.mColumn >= start && cursorPos.mColumn <= end) {
                    outIndex = outCount;
                }
            }

            found = lines[i].find(query, found + 1);
        }
    }
}

void ApplyGlobalTheme(int themeIndex) {
    if (themeIndex == 1) { // Jasny
        ImGui::StyleColorsLight();
    } else if (themeIndex == 2) { // Retro Blue
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.24f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.28f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.50f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.70f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.80f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.90f, 1.00f);
    } else { // Ciemny
        ImGui::StyleColorsDark();
    }
}