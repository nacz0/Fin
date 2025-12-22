#pragma once
#include "../Core/AppConfig.h"
#include "../Core/LSPClient.h"
#include "../Editor/EditorTab.h"
#include "imgui.h"
#include <vector>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

struct FileEntry {
    std::string name;
    std::string path;
    std::string extension;
    bool isDirectory;
};

void ShowExplorer(LSPClient& lsp, AppConfig& config, fs::path& currentPath, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus, float textScale, ImGuiIO& io);
