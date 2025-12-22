#pragma once
#include "../Editor/EditorTab.h"
#include "../Core/Compiler.h"
#include <vector>
#include <memory>
#include <string>

void ShowConsole(bool isCompiling, const std::string& compilationOutput, std::vector<ParsedError>& errorList, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus);

