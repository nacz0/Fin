#pragma once

#include <string>

namespace fin {

void InitializeI18n(const std::string& preferredLocale);
std::string NormalizeLocale(const std::string& locale);
void SetLocale(const std::string& locale);
std::string GetLocale();
bool IsEnglishLocale();

} // namespace fin
