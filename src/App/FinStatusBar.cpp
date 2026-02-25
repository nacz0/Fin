#include "App/FinStatusBar.h"

namespace fin {

void RenderStatusBar(
    fst::Context& ctx,
    float windowW,
    float windowH,
    float statusBarHeight,
    const std::string& statusText,
    const std::vector<std::unique_ptr<DocumentTab>>& docs,
    int activeTab) {
    fst::DrawList& dl = ctx.drawList();
    const fst::Theme& theme = ctx.theme();
    fst::Rect statusRect(0.0f, windowH - statusBarHeight, windowW, statusBarHeight);
    dl.addRectFilled(statusRect, theme.colors.panelBackground.darker(0.1f));
    dl.addLine(
        fst::Vec2(statusRect.x(), statusRect.y()),
        fst::Vec2(statusRect.right(), statusRect.y()),
        theme.colors.border);

    if (!ctx.font()) {
        return;
    }

    dl.addText(ctx.font(), fst::Vec2(8.0f, statusRect.y() + 4.0f), statusText, theme.colors.textSecondary);
    if (activeTab < 0 || activeTab >= static_cast<int>(docs.size())) {
        return;
    }

    fst::TextPosition cur = docs[activeTab]->editor.cursor();
    int diagCount = static_cast<int>(docs[activeTab]->lspDiagnostics.size());
    std::string rightText = fst::i18n("statusbar.line") + " " + std::to_string(cur.line + 1) +
                            ", " + fst::i18n("statusbar.col") + " " + std::to_string(cur.column + 1) +
                            " | " + fst::i18n("statusbar.diag") + " " + std::to_string(diagCount) +
                            " | " + (docs[activeTab]->path.empty() ? fst::i18n("statusbar.new_file") : docs[activeTab]->path);
    dl.addText(ctx.font(), fst::Vec2(320.0f, statusRect.y() + 4.0f), rightText, theme.colors.textSecondary);
}

} // namespace fin
