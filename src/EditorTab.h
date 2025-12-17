#pragma once
#include <string>
#include "TextEditor.h" // Musi widzieć edytor, żeby go użyć

struct EditorTab {
    std::string name;
    std::string path;
    TextEditor editor;
    bool isOpen = true;

    EditorTab() {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    }
};