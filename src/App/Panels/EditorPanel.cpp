#include "App/Panels/EditorPanel.h"

#include "App/FinHelpers.h"
#include "fastener/fastener.h"

#include <algorithm>
#include <map>
#include <vector>

namespace fin {

namespace {

bool ColorsEqual(const fst::Color& lhs, const fst::Color& rhs) {
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b &&
           lhs.a == rhs.a;
}

std::vector<size_t> FindAllMatches(const std::string& text, const std::string& query) {
    std::vector<size_t> offsets;
    if (query.empty() || text.empty()) {
        return offsets;
    }

    size_t pos = text.find(query);
    while (pos != std::string::npos) {
        offsets.push_back(pos);
        pos = text.find(query, pos + 1);
    }
    return offsets;
}

std::vector<fst::TextSegment> BuildSearchStyledSegments(
    const std::string& lineText,
    const std::vector<fst::TextSegment>& baseSegments,
    const std::string& query,
    const fst::Color& defaultTextColor,
    const fst::Color& matchBackground) {
    std::vector<fst::TextSegment> result;
    const int lineLength = static_cast<int>(lineText.size());
    if (lineLength <= 0) {
        return result;
    }

    std::vector<fst::Color> colors(static_cast<size_t>(lineLength), defaultTextColor);
    std::vector<fst::Color> backgrounds(static_cast<size_t>(lineLength), fst::Color::transparent());

    const auto paintColorRange = [&](int start, int end, const fst::Color& color) {
        const int from = std::clamp(start, 0, lineLength);
        const int to = std::clamp(end, 0, lineLength);
        for (int i = from; i < to; ++i) {
            colors[static_cast<size_t>(i)] = color;
        }
    };

    const auto paintBackgroundRange = [&](int start, int end, const fst::Color& background) {
        const int from = std::clamp(start, 0, lineLength);
        const int to = std::clamp(end, 0, lineLength);
        for (int i = from; i < to; ++i) {
            backgrounds[static_cast<size_t>(i)] = background;
        }
    };

    for (const fst::TextSegment& segment : baseSegments) {
        paintColorRange(segment.startColumn, segment.endColumn, segment.color);
    }

    if (!query.empty()) {
        size_t pos = lineText.find(query);
        while (pos != std::string::npos) {
            const int start = static_cast<int>(pos);
            const int end = start + static_cast<int>(query.size());
            paintBackgroundRange(start, end, matchBackground);
            pos = lineText.find(query, pos + 1);
        }
    }

    int i = 0;
    while (i < lineLength) {
        const fst::Color color = colors[static_cast<size_t>(i)];
        const fst::Color background = backgrounds[static_cast<size_t>(i)];
        const int start = i;
        ++i;
        while (i < lineLength) {
            if (!ColorsEqual(colors[static_cast<size_t>(i)], color)) {
                break;
            }
            if (!ColorsEqual(backgrounds[static_cast<size_t>(i)], background)) {
                break;
            }
            ++i;
        }

        result.push_back({start, i, color, background});
    }

    return result;
}

void ApplySearchAwareStyle(DocumentTab& tab, const fst::Theme& theme) {
    const std::string pathOrName = tab.path.empty() ? tab.name : tab.path;
    if (!tab.findVisible || tab.findQuery.empty()) {
        applyCppSyntaxHighlighting(tab.editor, pathOrName, theme);
        return;
    }

    const bool cppLike = isCppLikePath(pathOrName);
    const std::string query = tab.findQuery;
    const fst::Color matchBackground(
        theme.colors.warning.r,
        theme.colors.warning.g,
        theme.colors.warning.b,
        95);

    tab.editor.setStyleProvider([cppLike, query, theme, matchBackground](int, const std::string& lineText) {
        const std::vector<fst::TextSegment> baseSegments =
            cppLike ? colorizeCppSnippet(lineText, theme) : std::vector<fst::TextSegment>{};
        return BuildSearchStyledSegments(lineText, baseSegments, query, theme.colors.text, matchBackground);
    });
}

bool RenderFindBar(
    fst::Context& ctx,
    DocumentTab& tab,
    const fst::Rect& barRect,
    bool& requestNextMatch,
    bool& requestPreviousMatch) {
    const fst::Theme& theme = ctx.theme();
    fst::IDrawList& dl = ctx.drawList();

    dl.addRectFilled(barRect, theme.colors.panelBackground);
    dl.addRect(barRect, theme.colors.border);

    const float padding = 6.0f;
    const float gap = 6.0f;
    const float controlHeight = std::max(20.0f, barRect.height() - padding * 2.0f);
    const float buttonWidth = 86.0f;
    const float counterReserve = 80.0f;
    const float reservedWidth = buttonWidth * 2.0f + counterReserve + gap * 3.0f;
    const float inputWidth = std::max(10.0f, barRect.width() - padding * 2.0f - reservedWidth);

    const std::string inputId = "editor_find_input_" + tab.id;
    if (tab.findFocusPending) {
        ctx.setFocusedWidget(ctx.makeId(inputId));
        tab.findFocusPending = false;
    }

    fst::TextInputOptions inputOptions;
    inputOptions.style = fst::Style()
                             .withPos(barRect.x() + padding, barRect.y() + padding)
                             .withSize(inputWidth, controlHeight);
    const bool queryChanged = fst::TextInput(ctx, inputId, tab.findQuery, inputOptions);

    const float nextX = barRect.x() + padding + inputWidth + gap;
    fst::ButtonOptions nextOptions;
    nextOptions.style = fst::Style().withPos(nextX, barRect.y() + padding).withSize(buttonWidth, controlHeight);
    if (fst::Button(ctx, fst::i18n("find.next"), nextOptions)) {
        requestNextMatch = true;
    }

    const float prevX = nextX + buttonWidth + gap;
    fst::ButtonOptions previousOptions;
    previousOptions.style = fst::Style().withPos(prevX, barRect.y() + padding).withSize(buttonWidth, controlHeight);
    if (fst::Button(ctx, fst::i18n("find.previous"), previousOptions)) {
        requestPreviousMatch = true;
    }

    const std::vector<size_t> matches = FindAllMatches(tab.editor.getText(), tab.findQuery);
    const int safeIndex = (tab.findMatchIndex >= 0 && tab.findMatchIndex < static_cast<int>(matches.size()))
        ? tab.findMatchIndex + 1
        : (matches.empty() ? 0 : 1);
    const std::string counterText =
        "(" + std::to_string(safeIndex) + "/" + std::to_string(matches.size()) + ")";

    if (fst::Font* font = ctx.font()) {
        const fst::Vec2 counterSize = font->measureText(counterText);
        const fst::Vec2 counterPos(
            prevX + buttonWidth + gap,
            barRect.y() + (barRect.height() - counterSize.y) * 0.5f);
        dl.addText(font, counterPos, counterText, theme.colors.text);
    }

    const fst::WidgetState inputState = fst::getWidgetState(ctx, ctx.makeId(inputId));
    const fst::InputState& input = ctx.input();
    if (inputState.focused && (input.isKeyPressed(fst::Key::Enter) || input.isKeyPressed(fst::Key::KPEnter))) {
        if (input.modifiers().shift) {
            requestPreviousMatch = true;
        } else {
            requestNextMatch = true;
        }
    }
    if (inputState.focused && input.isKeyPressed(fst::Key::Escape)) {
        tab.findVisible = false;
    }

    return queryChanged;
}

} // namespace

void RenderEditorPanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    fst::TabControl& tabControl,
    float textScale,
    bool completionVisible,
    int completionOwnerTab,
    const std::string& completionOwnerDocumentPath,
    bool lspActive,
    LSPClient& lsp,
    const ClampActiveTabFn& clampActiveTab,
    const CloseTabFn& closeTab,
    const CloseCompletionPopupFn& closeCompletionPopup) {
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.editor"))) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    tabControl.clearTabs();
    for (const auto& tabPtr : docs) {
        const DocumentTab& tab = *tabPtr;
        tabControl.addTab(tab.id, tab.name, true);
        if (fst::TabItem* item = tabControl.getTab(tabControl.tabCount() - 1)) {
            item->modified = tab.dirty;
        }
    }

    clampActiveTab();
    if (activeTab >= 0) {
        tabControl.selectTab(activeTab);
    }

    int closeRequested = -1;
    fst::TabControlEvents tabEvents;
    tabEvents.onSelect = [&](int index, const fst::TabItem&) {
        activeTab = index;
        if (completionVisible &&
            (completionOwnerTab != activeTab ||
             (activeTab >= 0 && activeTab < static_cast<int>(docs.size()) &&
              docs[activeTab]->lspDocumentPath != completionOwnerDocumentPath))) {
            closeCompletionPopup();
        }
    };
    tabEvents.onClose = [&](int index, const fst::TabItem&) { closeRequested = index; };

    fst::TabControlOptions tabOptions;
    tabOptions.tabHeight = 30.0f;
    tabOptions.showCloseButtons = true;

    fst::Rect editorArea = tabControl.render(ctx, "editor_tabs", bounds, tabOptions, tabEvents);
    clampActiveTab();

    if (activeTab >= 0 && activeTab < static_cast<int>(docs.size())) {
        DocumentTab& activeDoc = *docs[activeTab];
        bool requestNextMatch = false;
        bool requestPreviousMatch = false;
        bool findQueryChanged = false;

        if (activeDoc.findVisible) {
            const float findBarHeight = 38.0f;
            const fst::Rect findBarRect(editorArea.x(), editorArea.y(), editorArea.width(), findBarHeight);
            findQueryChanged = RenderFindBar(ctx, activeDoc, findBarRect, requestNextMatch, requestPreviousMatch);
            editorArea = fst::Rect(
                editorArea.x(),
                editorArea.y() + findBarHeight,
                editorArea.width(),
                std::max(0.0f, editorArea.height() - findBarHeight));
        }

        const std::string textBeforeRender = activeDoc.editor.getText();
        const std::vector<size_t> findMatches = FindAllMatches(textBeforeRender, activeDoc.findQuery);
        if (!activeDoc.findVisible || activeDoc.findQuery.empty()) {
            activeDoc.findMatchIndex = -1;
        } else {
            if (findQueryChanged) {
                activeDoc.findMatchIndex = -1;
            }

            bool jumpToMatch = false;
            if (findMatches.empty()) {
                activeDoc.findMatchIndex = -1;
            } else {
                if (activeDoc.findMatchIndex < 0 || activeDoc.findMatchIndex >= static_cast<int>(findMatches.size())) {
                    const size_t cursorOffset = offsetFromPosition(textBeforeRender, activeDoc.editor.cursor());
                    const auto it = std::lower_bound(findMatches.begin(), findMatches.end(), cursorOffset);
                    activeDoc.findMatchIndex = (it == findMatches.end())
                        ? 0
                        : static_cast<int>(std::distance(findMatches.begin(), it));
                    jumpToMatch = true;
                }
                if (requestPreviousMatch) {
                    activeDoc.findMatchIndex =
                        (activeDoc.findMatchIndex - 1 + static_cast<int>(findMatches.size())) %
                        static_cast<int>(findMatches.size());
                    jumpToMatch = true;
                } else if (requestNextMatch || findQueryChanged) {
                    if (!findQueryChanged) {
                        activeDoc.findMatchIndex = (activeDoc.findMatchIndex + 1) % static_cast<int>(findMatches.size());
                    }
                    jumpToMatch = true;
                }

                if (jumpToMatch) {
                    activeDoc.editor.setCursor(
                        positionFromOffset(textBeforeRender, findMatches[static_cast<size_t>(activeDoc.findMatchIndex)]));
                }
            }
        }

        ApplySearchAwareStyle(activeDoc, ctx.theme());

        std::map<int, LSPDiagnostic> bestDiagnosticByLine;
        for (const LSPDiagnostic& diag : activeDoc.lspDiagnostics) {
            if (diag.line < 0) {
                continue;
            }
            auto it = bestDiagnosticByLine.find(diag.line);
            if (it == bestDiagnosticByLine.end() || diag.severity < it->second.severity) {
                bestDiagnosticByLine[diag.line] = diag;
            }
        }

        std::vector<fst::TextLineAnnotation> lineAnnotations;
        lineAnnotations.reserve(bestDiagnosticByLine.size());
        for (const auto& [lineIndex, diag] : bestDiagnosticByLine) {
            fst::TextLineAnnotation annotation;
            annotation.line = lineIndex;
            if (diag.severity == 1) {
                annotation.highlightColor = fst::Color(170, 24, 12, 165);
                annotation.tooltipTitle = "Error at line " + std::to_string(lineIndex + 1) + ":";
            } else if (diag.severity == 2) {
                annotation.highlightColor = fst::Color(150, 96, 0, 130);
                annotation.tooltipTitle = "Warning at line " + std::to_string(lineIndex + 1) + ":";
            } else {
                annotation.highlightColor = fst::Color(18, 98, 150, 95);
                annotation.tooltipTitle = "Info at line " + std::to_string(lineIndex + 1) + ":";
            }
            annotation.tooltipMessage = diag.message;
            lineAnnotations.push_back(std::move(annotation));
        }
        activeDoc.editor.setLineAnnotations(std::move(lineAnnotations));

        if (editorArea.width() > 8.0f && editorArea.height() > 8.0f) {
            fst::TextEditorOptions editorOptions;
            editorOptions.fontSize = 14.0f * textScale;
            editorOptions.showLineNumbers = true;
            editorOptions.suppressNavigationKeys = completionVisible;
            activeDoc.editor.render(ctx, editorArea, editorOptions);
        }

        std::string currentText = activeDoc.editor.getText();
        activeDoc.dirty = (currentText != activeDoc.savedText);

        if (lspActive && !activeDoc.lspDocumentPath.empty()) {
            if (!activeDoc.lspOpened) {
                lsp.DidOpen(activeDoc.lspDocumentPath, currentText);
                activeDoc.lspOpened = true;
                activeDoc.lspTextSnapshot = currentText;
            } else if (currentText != activeDoc.lspTextSnapshot) {
                lsp.DidChange(activeDoc.lspDocumentPath, currentText);
                activeDoc.lspTextSnapshot = currentText;
            }
        }
    }

    if (closeRequested >= 0) {
        closeTab(closeRequested);
    }

    fst::EndDockableWindow(ctx);
}

} // namespace fin
