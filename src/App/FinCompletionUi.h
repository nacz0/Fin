#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/LSPClient.h"
#include "fastener/fastener.h"

#include <functional>
#include <string>
#include <vector>

namespace fin {

struct CompletionUiState {
    bool visible = false;
    bool loading = false;
    int selected = 0;
    float scrollOffset = 0.0f;
    bool scrollbarDragging = false;
    float scrollbarDragOffset = 0.0f;
    fst::Key repeatKey = fst::Key::Unknown;
    float repeatTimer = 0.0f;
    std::vector<LSPCompletionItem> items;
    int ownerTab = -1;
    std::string ownerDocumentPath;
};

struct CompletionWindowRenderResult {
    bool drawn = false;
    fst::Rect bounds;
};

void ResetCompletionNavigationState(CompletionUiState& state);
void ResetCompletionInteractionState(CompletionUiState& state);
void ShowCompletionPopup(
    CompletionUiState& state,
    int activeTab,
    std::vector<LSPCompletionItem> items,
    bool loading,
    std::string ownerPath);

void HandleCompletionKeyboardNavigation(
    fst::Context& ctx,
    fst::InputState& input,
    CompletionUiState& state,
    const std::function<void(const LSPCompletionItem&)>& applyCompletionItem);

CompletionWindowRenderResult RenderCompletionPopup(
    fst::Context& ctx,
    fst::InputState& input,
    CompletionUiState& state,
    const std::function<void(const LSPCompletionItem&)>& applyCompletionItem);

} // namespace fin
