#include "App/FinCompletionUi.h"

#include "App/FinHelpers.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace fin {

namespace {

constexpr float kCompletionListHeight = 260.0f;

float completionItemHeight(fst::Context& ctx) {
    if (ctx.font()) {
        return ctx.font()->lineHeight() + ctx.theme().metrics.paddingSmall * 2.0f;
    }
    return 24.0f;
}

void keepCompletionSelectionVisible(CompletionUiState& state, float itemHeight, float viewHeight) {
    if (state.items.empty()) {
        state.scrollOffset = 0.0f;
        return;
    }

    const float totalHeight = itemHeight * static_cast<float>(state.items.size());
    const float maxScroll = std::max(0.0f, totalHeight - viewHeight);
    state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScroll);

    const float selectedTop = state.selected * itemHeight;
    const float selectedBottom = selectedTop + itemHeight;
    if (selectedTop < state.scrollOffset) {
        state.scrollOffset = selectedTop;
    } else if (selectedBottom > state.scrollOffset + viewHeight) {
        state.scrollOffset = selectedBottom - viewHeight;
    }

    state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScroll);
}

bool shouldRepeatCompletionNav(fst::Context& ctx, fst::InputState& input, CompletionUiState& state, fst::Key key) {
    if (input.isKeyPressed(key)) {
        state.repeatKey = key;
        state.repeatTimer = 0.35f;
        return true;
    }

    if (state.repeatKey == key && input.isKeyDown(key)) {
        state.repeatTimer -= ctx.deltaTime();
        if (state.repeatTimer <= 0.0f) {
            state.repeatTimer = 0.05f;
            return true;
        }
    }

    if (state.repeatKey == key && !input.isKeyDown(key)) {
        state.repeatKey = fst::Key::Unknown;
        state.repeatTimer = 0.0f;
    }
    return false;
}

} // namespace

void ResetCompletionNavigationState(CompletionUiState& state) {
    state.scrollOffset = 0.0f;
    state.scrollbarDragging = false;
    state.scrollbarDragOffset = 0.0f;
    state.repeatKey = fst::Key::Unknown;
    state.repeatTimer = 0.0f;
}

void ResetCompletionInteractionState(CompletionUiState& state) {
    state.selected = 0;
    ResetCompletionNavigationState(state);
}

void ShowCompletionPopup(
    CompletionUiState& state,
    int activeTab,
    std::vector<LSPCompletionItem> items,
    bool loading,
    std::string ownerPath) {
    state.visible = true;
    state.loading = loading;
    state.items = std::move(items);
    ResetCompletionInteractionState(state);
    state.ownerTab = activeTab;
    state.ownerDocumentPath = std::move(ownerPath);
}

void HandleCompletionKeyboardNavigation(
    fst::Context& ctx,
    fst::InputState& input,
    CompletionUiState& state,
    const std::function<void(const LSPCompletionItem&)>& applyCompletionItem) {
    if (state.visible && !state.loading && !state.items.empty()) {
        state.selected = std::clamp(state.selected, 0, static_cast<int>(state.items.size()) - 1);
        const float itemHeight = completionItemHeight(ctx);
        bool selectionMovedByKeyboard = false;

        if (shouldRepeatCompletionNav(ctx, input, state, fst::Key::Down)) {
            state.selected = std::min(state.selected + 1, static_cast<int>(state.items.size()) - 1);
            selectionMovedByKeyboard = true;
        }
        if (shouldRepeatCompletionNav(ctx, input, state, fst::Key::Up)) {
            state.selected = std::max(state.selected - 1, 0);
            selectionMovedByKeyboard = true;
        }
        if (selectionMovedByKeyboard) {
            keepCompletionSelectionVisible(state, itemHeight, kCompletionListHeight);
        }
        if (input.isKeyPressed(fst::Key::Enter) || input.isKeyPressed(fst::Key::KPEnter)) {
            applyCompletionItem(state.items[state.selected]);
        }
    } else if (!state.visible) {
        state.repeatKey = fst::Key::Unknown;
        state.repeatTimer = 0.0f;
    }
}

CompletionWindowRenderResult RenderCompletionPopup(
    fst::Context& ctx,
    fst::InputState& input,
    CompletionUiState& state,
    const std::function<void(const LSPCompletionItem&)>& applyCompletionItem) {
    CompletionWindowRenderResult result;
    if (!state.visible) {
        return result;
    }

    fst::DockableWindowOptions completionOptions;
    completionOptions.open = &state.visible;
    completionOptions.allowDocking = false;
    completionOptions.allowFloating = true;
    completionOptions.draggable = false;
    completionOptions.showTitleBar = false;
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.completion"), completionOptions)) {
        return result;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    result.bounds = bounds;
    result.drawn = true;
    ctx.layout().beginContainer(bounds);

    if (state.loading) {
        fst::Label(ctx, fst::i18n("completion.fetching"));
    } else if (state.items.empty()) {
        fst::Label(ctx, fst::i18n("completion.none"));
    } else {
        state.selected = std::clamp(state.selected, 0, static_cast<int>(state.items.size()) - 1);
        const fst::Theme& theme = ctx.theme();
        fst::Font* font = ctx.font();
        fst::DrawList& dl = ctx.drawList();
        const float itemHeight = completionItemHeight(ctx);

        const float listWidth = std::max(260.0f, bounds.width() - 20.0f);
        const fst::Rect listRect = fst::Allocate(ctx, listWidth, kCompletionListHeight);

        const float totalHeight = itemHeight * static_cast<float>(state.items.size());
        const bool needsScrollbar = totalHeight > listRect.height();
        const float scrollbarWidth = needsScrollbar ? std::max(10.0f, theme.metrics.scrollbarWidth) : 0.0f;
        fst::Rect contentRect(listRect.x(), listRect.y(), listRect.width() - scrollbarWidth, listRect.height());

        const bool listHovered = listRect.contains(input.mousePos()) && !ctx.isOccluded(input.mousePos());
        if (listHovered) {
            state.scrollOffset -= input.scrollDelta().y * itemHeight;
        }
        const float maxScroll = std::max(0.0f, totalHeight - listRect.height());
        state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScroll);

        const float listRadius = std::max(2.0f, theme.metrics.borderRadiusSmall - 2.0f);
        const fst::Color listFillColor = theme.colors.inputBackground.darker(0.06f);
        dl.addRectFilled(listRect, listFillColor, listRadius);

        int hoveredIndex = -1;
        dl.pushClipRect(contentRect);
        for (int i = 0; i < static_cast<int>(state.items.size()); ++i) {
            const float rowY = contentRect.y() + i * itemHeight - state.scrollOffset;
            if (rowY + itemHeight < contentRect.y() || rowY > contentRect.bottom()) {
                continue;
            }

            const fst::Rect rowRect(contentRect.x(), rowY, contentRect.width(), itemHeight);
            const bool selected = (i == state.selected);
            const bool hovered = rowRect.contains(input.mousePos()) &&
                                 contentRect.contains(input.mousePos()) &&
                                 !ctx.isOccluded(input.mousePos());
            if (hovered) {
                hoveredIndex = i;
            }

            if (selected) {
                dl.addRectFilled(rowRect, theme.colors.selection);
            } else if (hovered) {
                dl.addRectFilled(rowRect, theme.colors.selection.withAlpha(static_cast<uint8_t>(90)));
            }

            if (font) {
                const LSPCompletionItem& item = state.items[i];
                const float textY = rowRect.y() + (itemHeight - font->lineHeight()) * 0.5f;
                float textX = rowRect.x() + theme.metrics.paddingSmall;
                const std::string rowText = item.detail.empty() ? item.label : (item.label + " :: " + item.detail);
                const std::vector<fst::TextSegment> syntax = colorizeCppSnippet(rowText, theme);

                auto toRowColor = [&](const fst::Color& base) -> fst::Color {
                    if (selected) {
                        return fst::Color::lerp(base, theme.colors.selectionText, 0.55f);
                    }
                    return base;
                };

                int cursor = 0;
                const int rowTextLen = static_cast<int>(rowText.size());
                for (const auto& seg : syntax) {
                    const int start = std::clamp(seg.startColumn, 0, rowTextLen);
                    const int end = std::clamp(seg.endColumn, start, rowTextLen);
                    if (start > cursor) {
                        const std::string plain = rowText.substr(static_cast<size_t>(cursor), static_cast<size_t>(start - cursor));
                        dl.addText(font, fst::Vec2(textX, textY), plain, toRowColor(theme.colors.text));
                        textX += font->measureText(plain).x;
                    }
                    if (end > start) {
                        const std::string token = rowText.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
                        dl.addText(font, fst::Vec2(textX, textY), token, toRowColor(seg.color));
                        textX += font->measureText(token).x;
                    }
                    cursor = end;
                }

                if (cursor < static_cast<int>(rowText.size())) {
                    const std::string plain = rowText.substr(static_cast<size_t>(cursor));
                    dl.addText(font, fst::Vec2(textX, textY), plain, toRowColor(theme.colors.text));
                }
            }
        }
        dl.popClipRect();

        if (needsScrollbar) {
            const fst::Rect track(listRect.right() - scrollbarWidth, listRect.y(), scrollbarWidth, listRect.height());
            const float thumbHeight = std::max(20.0f, (listRect.height() / totalHeight) * listRect.height());
            float thumbY = track.y();
            if (maxScroll > 0.001f) {
                const float t = std::clamp(state.scrollOffset / maxScroll, 0.0f, 1.0f);
                thumbY += t * (track.height() - thumbHeight);
            }
            const fst::Rect thumb(track.x() + 2.0f, thumbY, track.width() - 4.0f, thumbHeight);

            if (input.isMousePressedRaw(fst::MouseButton::Left) && !ctx.isOccluded(input.mousePos())) {
                if (thumb.contains(input.mousePos())) {
                    state.scrollbarDragging = true;
                    state.scrollbarDragOffset = input.mousePos().y - thumb.y();
                } else if (track.contains(input.mousePos())) {
                    const float newThumbTop = std::clamp(
                        input.mousePos().y - track.y() - thumbHeight * 0.5f,
                        0.0f,
                        track.height() - thumbHeight);
                    const float t = (track.height() - thumbHeight) > 0.0f
                        ? (newThumbTop / (track.height() - thumbHeight))
                        : 0.0f;
                    state.scrollOffset = t * maxScroll;
                    state.scrollbarDragging = true;
                    state.scrollbarDragOffset = thumbHeight * 0.5f;
                }
            }

            if (state.scrollbarDragging) {
                if (input.isMouseDown(fst::MouseButton::Left)) {
                    const float newThumbTop = std::clamp(
                        input.mousePos().y - track.y() - state.scrollbarDragOffset,
                        0.0f,
                        track.height() - thumbHeight);
                    const float t = (track.height() - thumbHeight) > 0.0f
                        ? (newThumbTop / (track.height() - thumbHeight))
                        : 0.0f;
                    state.scrollOffset = t * maxScroll;
                } else {
                    state.scrollbarDragging = false;
                }
            }

            dl.addRectFilled(track, theme.colors.scrollbarTrack);
            const float updatedThumbY = (maxScroll > 0.001f)
                ? (track.y() + (state.scrollOffset / maxScroll) * (track.height() - thumbHeight))
                : track.y();
            const fst::Rect updatedThumb(track.x() + 2.0f, updatedThumbY, track.width() - 4.0f, thumbHeight);
            const fst::Color thumbColor = (state.scrollbarDragging || track.contains(input.mousePos()))
                ? theme.colors.scrollbarThumbHover
                : theme.colors.scrollbarThumb;
            dl.addRectFilled(updatedThumb, thumbColor, std::max(2.0f, (scrollbarWidth - 4.0f) * 0.5f));
        } else {
            state.scrollbarDragging = false;
        }

        if (hoveredIndex >= 0 && input.isMousePressedRaw(fst::MouseButton::Left)) {
            state.selected = hoveredIndex;
            applyCompletionItem(state.items[state.selected]);
        }

        if (state.selected >= 0 && state.selected < static_cast<int>(state.items.size())) {
            const LSPCompletionItem& selectedItem = state.items[state.selected];
            const bool localItem = selectedItem.detail == "local";

            fst::LabelOptions sourceOptions;
            sourceOptions.color = localItem ? ctx.theme().colors.textSecondary : ctx.theme().colors.primary;
            fst::Label(ctx, localItem ? fst::i18n("completion.source.local") : fst::i18n("completion.source.lsp"), sourceOptions);
            if (!selectedItem.detail.empty() && !localItem) {
                fst::LabelSecondary(ctx, fst::i18n("completion.details", {selectedItem.detail}));
            }
        }

        fst::LabelSecondary(ctx, fst::i18n("completion.hint"));
    }

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
    return result;
}

} // namespace fin
