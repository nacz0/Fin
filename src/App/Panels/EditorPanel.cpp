#include "App/Panels/EditorPanel.h"

#include "fastener/fastener.h"

#include <algorithm>
#include <map>

namespace fin {

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
        std::map<int, LSPDiagnostic> bestDiagnosticByLine;
        for (const LSPDiagnostic& diag : docs[activeTab]->lspDiagnostics) {
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
        docs[activeTab]->editor.setLineAnnotations(std::move(lineAnnotations));

        fst::TextEditorOptions editorOptions;
        editorOptions.fontSize = 14.0f * textScale;
        editorOptions.showLineNumbers = true;
        editorOptions.suppressNavigationKeys = completionVisible;
        docs[activeTab]->editor.render(ctx, editorArea, editorOptions);

        std::string currentText = docs[activeTab]->editor.getText();
        docs[activeTab]->dirty = (currentText != docs[activeTab]->savedText);

        if (lspActive && !docs[activeTab]->lspDocumentPath.empty()) {
            if (!docs[activeTab]->lspOpened) {
                lsp.DidOpen(docs[activeTab]->lspDocumentPath, currentText);
                docs[activeTab]->lspOpened = true;
                docs[activeTab]->lspTextSnapshot = currentText;
            } else if (currentText != docs[activeTab]->lspTextSnapshot) {
                lsp.DidChange(docs[activeTab]->lspDocumentPath, currentText);
                docs[activeTab]->lspTextSnapshot = currentText;
            }
        }
    }

    if (closeRequested >= 0) {
        closeTab(closeRequested);
    }

    fst::EndDockableWindow(ctx);
}

} // namespace fin
