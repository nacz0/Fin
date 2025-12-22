#pragma once
#include "../Core/AppConfig.h"
#include "../Core/LSPClient.h"
#include "../Editor/EditorTab.h"
#include <vector>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

void ShowMainMenuBar(LSPClient& lsp, AppConfig& config, std::vector<std::unique_ptr<EditorTab>>& tabs, int activeTab, fs::path& currentPath, int& nextTabToFocus, bool& actionNew, bool& actionOpen, bool& actionSave, bool& actionCloseTab, bool& actionSearch, bool& actionBuild);
