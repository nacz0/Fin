#include "App/Panels/ConsolePanel.h"

#include "fastener/fastener.h"

#include <algorithm>

namespace fin {

void RenderConsolePanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    const std::vector<ParsedError>& errorList,
    std::string& compilationOutput,
    const OpenDocumentFn& openDocument,
    const ClampActiveTabFn& clampActiveTab) {
    if (!fst::BeginDockableWindow(ctx, "Konsola")) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);

    if (activeTab >= 0 && activeTab < static_cast<int>(docs.size()) && !docs[activeTab]->lspDiagnostics.empty()) {
        fst::Label(ctx, "Diagnostyka LSP:");
        for (const LSPDiagnostic& diag : docs[activeTab]->lspDiagnostics) {
            bool selected = false;
            std::string level = diag.severity <= 1 ? "Error" : "Warning";
            std::string line = "L" + std::to_string(diag.line + 1) + ": " + level + ": " + diag.message;
            if (fst::Selectable(ctx, line, selected)) {
                fst::TextPosition pos;
                pos.line = std::max(0, diag.line);
                pos.column = std::max(0, diag.range_start);
                docs[activeTab]->editor.setCursor(pos);
            }
        }
        fst::Separator(ctx);
    }

    if (!errorList.empty()) {
        fst::Label(ctx, "Bledy kompilatora:");
        for (const ParsedError& err : errorList) {
            bool selected = false;
            std::string line = err.fullMessage;
            if (!err.filename.empty() && err.line > 0) {
                line = err.filename + ":" + std::to_string(err.line) + ":" + std::to_string(err.col) + " " + err.message;
            }

            if (fst::Selectable(ctx, line, selected) && !err.filename.empty() && err.line > 0) {
                if (openDocument(err.filename)) {
                    clampActiveTab();
                    if (activeTab >= 0) {
                        fst::TextPosition pos;
                        pos.line = std::max(0, err.line - 1);
                        pos.column = std::max(0, err.col - 1);
                        docs[activeTab]->editor.setCursor(pos);
                    }
                }
            }
        }
    }

    fst::TextAreaOptions outputOptions;
    outputOptions.readonly = true;
    outputOptions.wordWrap = false;
    outputOptions.height = std::max(140.0f, bounds.height() - 140.0f);
    fst::TextArea(ctx, "build_output", compilationOutput, outputOptions);

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin