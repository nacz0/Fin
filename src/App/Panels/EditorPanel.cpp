#include "App/Panels/EditorPanel.h"

#include "fastener/fastener.h"

#include <algorithm>

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
    if (!fst::BeginDockableWindow(ctx, "Edytor")) {
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
