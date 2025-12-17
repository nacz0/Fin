#pragma once // To sprawia, że plik dołączy się tylko raz (zamiast się dublować)
#include <string>

// Deklaracje funkcji (obietnica, że one istnieją)
void SaveFile(const std::string& filename, const std::string& text);
std::string OpenFile(const std::string& filename);
std::string ExecCommand(const char* cmd);