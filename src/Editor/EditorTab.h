#pragma once
#include <string>
#include <mutex>
#include <memory>
#include "TextEditor.h"

#include "../Core/AppConfig.h"
#include "../Core/LSPClient.h"

struct EditorTab {
    const AppConfig* configRef = nullptr;
    std::string name;
    std::string path;
    TextEditor editor;
    bool isOpen = true;

    // --- Search state ---
    bool showSearch = false;
    char searchBuf[256] = ""; 
    bool searchFocus = false;
    int searchMatchCount = 0;
    int searchMatchIndex = 0;

    // --- LSP Diagnostics ---
    std::vector<LSPDiagnostic> lspDiagnostics;

    // --- Autocomplete ---
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
        bool justConsumedEnter = false; // Flaga do blokowania nowej linii po Enterze
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