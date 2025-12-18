#pragma once
#include <vector>
#include "EditorTab.h"
#include "imgui.h"

class ThemeManager {
public:
    static void ApplyTheme(int themeIndex, std::vector<EditorTab>& tabs);
    static void ApplyTheme(int themeIndex, EditorTab& tab);
};
