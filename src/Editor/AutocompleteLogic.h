#pragma once
#include "EditorTab.h"
#include "../Core/LSPClient.h"
#include "../Core/AppConfig.h"

void HandleAutocompleteLogic(EditorTab& tab, LSPClient& lsp, const AppConfig& config);
