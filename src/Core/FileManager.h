#pragma once
#include <string>

void SaveFile(const std::string& filename, const std::string& text);
std::string OpenFile(const std::string& filename);
std::string ExecCommand(const char* cmd);
std::string ShowOpenFileDialog(const std::string& initialDir);
std::string ShowSaveFileDialog(const std::string& suggestedName, const std::string& initialDir);
