#pragma once
#include "TextEditor.h"
#include <string>
extern std::string g_SearchLog;
// Funkcja obsługująca Backspace i Auto-domykanie (wywoływana PRZED Render)
void HandlePreRenderLogic(TextEditor& editor);

// Funkcja obsługująca Smart Enter (wywoływana PO Render)
void HandlePostRenderLogic(TextEditor& editor);

// Funkcja wyszukująca tekst w edytorze
void FindNext(TextEditor& editor, const std::string& query);
void FindPrev(TextEditor& editor, const std::string& query);