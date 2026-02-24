#pragma once

#include "fastener/fastener.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fin {

struct ExplorerEntry {
    std::filesystem::path path;
    std::string name;
    bool isDirectory = false;
};

void trimBuffer(std::string& text, size_t maxBytes);

void applyTheme(fst::Context& ctx, int themeId);
void applyCppSyntaxHighlighting(fst::TextEditor& editor, const std::string& pathOrName, const fst::Theme& theme);
std::vector<fst::TextSegment> colorizeCppSnippet(const std::string& text, const fst::Theme& theme);

std::string normalizePath(const std::string& path);
std::string uriToPath(const std::string& uri);

std::vector<ExplorerEntry> listEntries(const std::filesystem::path& root);

size_t offsetFromPosition(const std::string& text, const fst::TextPosition& pos);
std::string insertAtPosition(
    const std::string& text,
    const fst::TextPosition& pos,
    const std::string& insertion,
    fst::TextPosition& outCursor);

} // namespace fin
