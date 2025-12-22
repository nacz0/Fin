#pragma once
#include "TextEditor.h"
#include "../Core/AppConfig.h"
#include <string>

struct EditorTab;

// Pre-render logic (backspace, auto-closing brackets)
void HandlePreRenderLogic(TextEditor& editor, const AppConfig& config);

// Post-render logic (smart enter)
void HandlePostRenderLogic(EditorTab& tab);

// Global theme application
void ApplyGlobalTheme(int themeIndex);
