#pragma once
#include "TextEditor.h"
#include <string>

void FindNext(TextEditor& editor, const std::string& query);
void FindPrev(TextEditor& editor, const std::string& query);
void UpdateSearchInfo(TextEditor& editor, const std::string& query, int& outCount, int& outIndex);
