#pragma once
#include "../Core/AppConfig.h"
#include "../Core/LSPClient.h"
#include "../Editor/EditorTab.h"
#include <vector>
#include <memory>

void ShowSettings(AppConfig& config, float& textScale, LSPClient& lsp, std::vector<std::unique_ptr<EditorTab>>& tabs);
