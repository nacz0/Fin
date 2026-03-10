#pragma once

#include "fastener/fastener.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace fin {

struct ExplorerEntry {
    std::filesystem::path path;
    std::string name;
    bool isDirectory = false;
};

void trimBuffer(std::string& text, size_t maxBytes);

void applyTheme(fst::Context& ctx, int themeId);
bool isCppLikePath(const std::string& pathOrName);
void applyCppSyntaxHighlighting(fst::TextEditor& editor, const std::string& pathOrName, const fst::Theme& theme);
std::vector<fst::TextSegment> colorizeCppSnippet(const std::string& text, const fst::Theme& theme);

std::string normalizePath(const std::string& path);
std::string uriToPath(const std::string& uri);

std::vector<ExplorerEntry> listEntries(const std::filesystem::path& root);

void beginScrollablePanelContent(fst::Context& ctx, std::string_view id, const fst::Rect& bounds);
void endScrollablePanelContent(fst::Context& ctx, std::string_view id, const fst::Rect& bounds);

size_t offsetFromPosition(const std::string& text, const fst::TextPosition& pos);
fst::TextPosition positionFromOffset(const std::string& text, size_t offset);
std::string insertAtPosition(
    const std::string& text,
    const fst::TextPosition& pos,
    const std::string& insertion,
    fst::TextPosition& outCursor);

} // namespace fin
