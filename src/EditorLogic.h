#pragma once
#include "TextEditor.h"

// Funkcja obsługująca Backspace i Auto-domykanie (wywoływana PRZED Render)
void HandlePreRenderLogic(TextEditor& editor);

// Funkcja obsługująca Smart Enter (wywoływana PO Render)
void HandlePostRenderLogic(TextEditor& editor);