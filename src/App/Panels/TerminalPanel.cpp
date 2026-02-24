#include "App/Panels/TerminalPanel.h"

#include "fastener/fastener.h"

#include <algorithm>

namespace fin {

void RenderTerminalPanel(
    fst::Context& ctx,
    Terminal& terminal,
    std::string& terminalHistory,
    std::string& terminalInput) {
    if (!fst::BeginDockableWindow(ctx, "Terminal")) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);

    fst::TextAreaOptions terminalOptions;
    terminalOptions.readonly = true;
    terminalOptions.wordWrap = false;
    terminalOptions.height = std::max(140.0f, bounds.height() - 80.0f);
    fst::TextArea(ctx, "terminal_history", terminalHistory, terminalOptions);

    fst::BeginHorizontal(ctx, 8.0f);
    fst::TextInputOptions inputOptions;
    inputOptions.style = fst::Style().withWidth(std::max(120.0f, bounds.width() - 110.0f));
    (void)fst::TextInput(ctx, "terminal_input", terminalInput, inputOptions);
    if (fst::Button(ctx, "Wyslij") && !terminalInput.empty()) {
        terminal.SendInput(terminalInput);
        terminalInput.clear();
    }
    fst::EndHorizontal(ctx);

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin