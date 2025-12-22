#pragma once
#include "TextEditor.h"
#include <string>
#include "Utils.h"
struct EditorTab;
class LSPClient;

// Funkcja obsługująca Backspace i Auto-domykanie (wywoływana PRZED Render)
void HandlePreRenderLogic(TextEditor& editor, const AppConfig& config);

// Funkcja obsługująca Smart Enter (wywoływana PO Render)
void HandlePostRenderLogic(EditorTab& tab);

// Funkcja wyszukująca tekst w edytorze
void FindNext(TextEditor& editor, const std::string& query);
void FindPrev(TextEditor& editor, const std::string& query);
void UpdateSearchInfo(TextEditor& editor, const std::string& query, int& outCount, int& outIndex);
void ApplyGlobalTheme(int themeIndex);

// Autouzupełnianie
struct EditorTab;
class LSPClient;
void HandleAutocompleteLogic(EditorTab& tab, LSPClient& lsp, const AppConfig& config);