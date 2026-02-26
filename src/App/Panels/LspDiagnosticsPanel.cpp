#include "App/Panels/LspDiagnosticsPanel.h"

#include "App/FinHelpers.h"
#include "fastener/fastener.h"

#include <algorithm>

namespace fin {

namespace {

fst::Color DiagnosticColor(const fst::Theme& theme, int severity) {
    switch (severity) {
        case 1:
            return theme.colors.error;
        case 2:
            return theme.colors.warning;
        default:
            return theme.colors.info;
    }
}

std::string DiagnosticLabel(int severity) {
    switch (severity) {
        case 1:
            return fst::i18n("lsp.severity.error");
        case 2:
            return fst::i18n("lsp.severity.warn");
        default:
            return fst::i18n("lsp.severity.info");
    }
}

} // namespace

void RenderLspDiagnosticsPanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int activeTab) {
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.lsp_diagnostics"))) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    beginScrollablePanelContent(ctx, "lsp_diagnostics_scroll", bounds);
    const fst::Theme& theme = ctx.theme();

    if (activeTab < 0 || activeTab >= static_cast<int>(docs.size())) {
        fst::LabelSecondary(ctx, fst::i18n("lsp.no_active_document"));
        endScrollablePanelContent(ctx, "lsp_diagnostics_scroll", bounds);
        fst::EndDockableWindow(ctx);
        return;
    }

    DocumentTab& tab = *docs[activeTab];
    const std::vector<LSPDiagnostic>& diagnostics = tab.lspDiagnostics;

    int errors = 0;
    int warnings = 0;
    int infos = 0;
    for (const LSPDiagnostic& diag : diagnostics) {
        if (diag.severity <= 1) {
            ++errors;
        } else if (diag.severity == 2) {
            ++warnings;
        } else {
            ++infos;
        }
    }

    const std::string source = tab.path.empty() ? tab.name : tab.path;
    fst::LabelOptions sourceOpt;
    sourceOpt.color = theme.colors.textSecondary;
    fst::Label(ctx, fst::i18n("lsp.file", {source}), sourceOpt);

    fst::BeginHorizontal(ctx, 12.0f);
    {
        fst::LabelOptions errorOpt;
        errorOpt.color = errors > 0 ? theme.colors.error : theme.colors.textSecondary;
        fst::Label(ctx, fst::i18n("lsp.errors", {std::to_string(errors)}), errorOpt);

        fst::LabelOptions warningOpt;
        warningOpt.color = warnings > 0 ? theme.colors.warning : theme.colors.textSecondary;
        fst::Label(ctx, fst::i18n("lsp.warnings", {std::to_string(warnings)}), warningOpt);

        fst::LabelOptions infoOpt;
        infoOpt.color = infos > 0 ? theme.colors.info : theme.colors.textSecondary;
        fst::Label(ctx, fst::i18n("lsp.infos", {std::to_string(infos)}), infoOpt);
    }
    fst::EndHorizontal(ctx);
    fst::Separator(ctx);

    if (diagnostics.empty()) {
        fst::LabelSecondary(ctx, fst::i18n("lsp.no_diagnostics"));
        endScrollablePanelContent(ctx, "lsp_diagnostics_scroll", bounds);
        fst::EndDockableWindow(ctx);
        return;
    }

    for (const LSPDiagnostic& diag : diagnostics) {
        const fst::Color diagColor = DiagnosticColor(theme, diag.severity);
        const std::string lineColumn = "L" + std::to_string(std::max(0, diag.line) + 1) +
                                       ":C" + std::to_string(std::max(0, diag.range_start) + 1);
        const std::string clickableLine = lineColumn + "  " + diag.message;

        fst::BeginHorizontal(ctx, 10.0f);
        {
            fst::LabelOptions severityOpt;
            severityOpt.color = diagColor;
            severityOpt.style = fst::Style().withWidth(62.0f);
            fst::Label(ctx, DiagnosticLabel(diag.severity), severityOpt);

            bool selected = false;
            if (fst::Selectable(ctx, clickableLine, selected)) {
                fst::TextPosition pos;
                pos.line = std::max(0, diag.line);
                pos.column = std::max(0, diag.range_start);
                tab.editor.setCursor(pos);
            }
        }
        fst::EndHorizontal(ctx);

        fst::Separator(ctx);
    }

    endScrollablePanelContent(ctx, "lsp_diagnostics_scroll", bounds);
    fst::EndDockableWindow(ctx);
}

} // namespace fin
