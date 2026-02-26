#include "App/Panels/TerminalPanel.h"

#include "fastener/fastener.h"

#include <algorithm>
#include <cmath>

namespace fin {

namespace {

void SubmitTerminalCommand(Terminal& terminal, std::string& terminalHistory, std::string& terminalInput) {
    std::string command = terminalInput;
    if (!command.empty() && command.back() == '\r') {
        command.pop_back();
    }

    // Keep typed command visible in history (cmd.exe is started with /Q and may not echo it back).
    terminalHistory += command;
    terminalHistory += '\n';

    terminal.SendInput(command);
    terminalInput.clear();
}

void EraseLastUtf8Codepoint(std::string& text) {
    if (text.empty()) {
        return;
    }

    size_t len = text.size();
    while (len > 0 && (static_cast<unsigned char>(text[len - 1]) & 0xC0) == 0x80) {
        --len;
    }
    if (len > 0) {
        --len;
    }
    text.resize(len);
}

} // namespace

void RenderTerminalPanel(
    fst::Context& ctx,
    Terminal& terminal,
    std::string& terminalHistory,
    std::string& terminalInput) {
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.terminal"))) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);

    const fst::WidgetId terminalInputId = ctx.makeId("terminal_console_input");
    (void)fst::handleWidgetInteraction(ctx, terminalInputId, bounds, true);
    const fst::WidgetState terminalState = fst::getWidgetState(ctx, terminalInputId);

    const auto& input = ctx.input();
    const bool clickedInsideTerminal =
        input.isMousePressedRaw(fst::MouseButton::Left) &&
        bounds.contains(input.mousePos()) &&
        !ctx.isOccluded(input.mousePos());
    if (clickedInsideTerminal) {
        ctx.setFocusedWidget(terminalInputId);
    }

    if (terminalState.focused) {
        const std::string& typed = input.textInput();
        if (!typed.empty()) {
            terminalInput += typed;
        }

        if (input.isKeyPressed(fst::Key::Backspace)) {
            EraseLastUtf8Codepoint(terminalInput);
        }

        if (input.isKeyPressed(fst::Key::Enter) || input.isKeyPressed(fst::Key::KPEnter)) {
            SubmitTerminalCommand(terminal, terminalHistory, terminalInput);
        }
    }

    const bool showCaret = terminalState.focused && (std::fmod(ctx.time() * 2.0f, 2.0f) < 1.0f);
    std::string terminalBuffer = terminalHistory;
    terminalBuffer += terminalInput;
    if (showCaret) {
        terminalBuffer += "_";
    }

    fst::TextAreaOptions terminalOptions;
    terminalOptions.readonly = true;
    terminalOptions.wordWrap = false;
    terminalOptions.style = fst::Style().withWidth(std::max(100.0f, bounds.width() - 8.0f));
    terminalOptions.height = std::max(140.0f, bounds.height() - 8.0f);
    fst::TextArea(ctx, "terminal_view", terminalBuffer, terminalOptions);
    if (clickedInsideTerminal) {
        ctx.setFocusedWidget(terminalInputId);
    }

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin
