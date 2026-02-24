#include "App/Panels/LspDiagnosticsPanel.h"

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

const char* DiagnosticLabel(int severity) {
    switch (severity) {
        case 1:
            return "ERROR";
        case 2:
            return "WARN";
        default:
            return "INFO";
    }
}

} // namespace

void RenderLspDiagnosticsPanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int activeTab) {
    if (!fst::BeginDockableWindow(ctx, "Diagnostyka LSP")) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);
    const fst::Theme& theme = ctx.theme();

    if (activeTab < 0 || activeTab >= static_cast<int>(docs.size())) {
        fst::LabelSecondary(ctx, "Brak aktywnego dokumentu.");
        ctx.layout().endContainer();
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
    fst::Label(ctx, "Plik: " + source, sourceOpt);

    fst::BeginHorizontal(ctx, 12.0f);
    {
        fst::LabelOptions errorOpt;
        errorOpt.color = errors > 0 ? theme.colors.error : theme.colors.textSecondary;
        fst::Label(ctx, "Error: " + std::to_string(errors), errorOpt);

        fst::LabelOptions warningOpt;
        warningOpt.color = warnings > 0 ? theme.colors.warning : theme.colors.textSecondary;
        fst::Label(ctx, "Warning: " + std::to_string(warnings), warningOpt);

        fst::LabelOptions infoOpt;
        infoOpt.color = infos > 0 ? theme.colors.info : theme.colors.textSecondary;
        fst::Label(ctx, "Info: " + std::to_string(infos), infoOpt);
    }
    fst::EndHorizontal(ctx);
    fst::Separator(ctx);

    if (diagnostics.empty()) {
        fst::LabelSecondary(ctx, "Brak diagnostyk LSP dla aktywnej karty.");
        ctx.layout().endContainer();
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

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin
