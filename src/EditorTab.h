#pragma once
#include <string>
#include <mutex>
#include <memory>
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
    int searchMatchCount = 0;
    int searchMatchIndex = 0;

    // --- NOWE: LSP ---
    struct Diagnostic {
        int line;
        std::string message;
        int severity;
    };
    std::vector<Diagnostic> lspDiagnostics;

    // --- NOWE: Autouzupełnianie z płynną pamięcią ---
    struct CompletionItem {
        std::string label;
        std::string detail;
        std::string insertText;
    };

    struct AutocompleteState {
        std::mutex mutex;
        std::vector<CompletionItem> items;
        std::vector<CompletionItem> pendingResults;
        bool hasNewResults = false;
        bool show = false;
        int selectedIndex = 0;
        TextEditor::Coordinates coord;
        bool requested = false;
    };

    std::shared_ptr<AutocompleteState> acState;

    EditorTab() : acState(std::make_shared<AutocompleteState>()) {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    }

    EditorTab(EditorTab&& other) noexcept = default;
    EditorTab& operator=(EditorTab&& other) noexcept = default;

    // Usuwamy konstruktory kopiujące
    EditorTab(const EditorTab&) = delete;
    EditorTab& operator=(const EditorTab&) = delete;
};