#pragma once
#include <string>
#include "TextEditor.h"

struct EditorTab {
    std::string name;
    std::string path;
    TextEditor editor;
    bool isOpen = true;

    // --- NOWE: Stan wyszukiwania ---
    bool showSearch = false;
    char searchBuf[256] = ""; 
    bool searchFocus = false; // Czy ustawić focus na pole tekstowe

    EditorTab() {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    }
};