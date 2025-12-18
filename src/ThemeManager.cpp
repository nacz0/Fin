#include "ThemeManager.h"
#include "EditorLogic.h"

void ThemeManager::ApplyTheme(int themeIndex, std::vector<EditorTab>& tabs) {
    ApplyGlobalTheme(themeIndex);
    
    const TextEditor::Palette* palette = &TextEditor::GetDarkPalette();
    if (themeIndex == 1) palette = &TextEditor::GetLightPalette();
    else if (themeIndex == 2) palette = &TextEditor::GetRetroBluePalette();
    
    for (auto& t : tabs) {
        ApplyTheme(themeIndex, t);
    }
}

void ThemeManager::ApplyTheme(int themeIndex, EditorTab& tab) {
    if (themeIndex >= 0) ApplyGlobalTheme(themeIndex);
    
    const TextEditor::Palette* palette = &TextEditor::GetDarkPalette();
    if (themeIndex == 1) palette = &TextEditor::GetLightPalette();
    else if (themeIndex == 2) palette = &TextEditor::GetRetroBluePalette();
    
    tab.editor.SetPalette(*palette);
}
