#pragma once
#include <memory>
#include "EditorTab.h"
#include "imgui.h"

class ThemeManager {
public:
    static void ApplyTheme(int themeIndex, std::vector<std::unique_ptr<EditorTab>>& tabs);
    static void ApplyTheme(int themeIndex, EditorTab& tab);
};
