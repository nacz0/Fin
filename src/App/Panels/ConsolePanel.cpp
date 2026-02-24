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
    const fst::Theme& theme = ctx.theme();

    int errorCount = 0;
    int warningCount = 0;
    for (const ParsedError& err : errorList) {
        if (err.isError) {
            ++errorCount;
        } else {
            ++warningCount;
        }
    }

    fst::Label(ctx, "Wynik kompilacji");
    fst::BeginHorizontal(ctx, 14.0f);
    {
        fst::LabelOptions errorsOpt;
        errorsOpt.color = errorCount > 0 ? theme.colors.error : theme.colors.success;
        fst::Label(ctx, "Bledy: " + std::to_string(errorCount), errorsOpt);

        fst::LabelOptions warningsOpt;
        warningsOpt.color = warningCount > 0 ? theme.colors.warning : theme.colors.textSecondary;
        fst::Label(ctx, "Ostrzezenia: " + std::to_string(warningCount), warningsOpt);
    }
    fst::EndHorizontal(ctx);
    fst::Separator(ctx);

    if (!errorList.empty()) {
        fst::Label(ctx, "Problemy kompilatora:");
        for (const ParsedError& err : errorList) {
            const bool canJump = !err.filename.empty() && err.line > 0;
            const std::string location = canJump
                ? (err.filename + ":" + std::to_string(err.line) + ":" + std::to_string(err.col))
                : "(bez lokalizacji)";
            const std::string message = err.message.empty() ? err.fullMessage : err.message;

            fst::BeginHorizontal(ctx, 10.0f);
            {
                fst::LabelOptions levelOpt;
                levelOpt.color = err.isError ? theme.colors.error : theme.colors.warning;
                levelOpt.style = fst::Style().withWidth(62.0f);
                fst::Label(ctx, err.isError ? "ERROR" : "WARN", levelOpt);

                fst::LabelOptions locationOpt;
                locationOpt.color = theme.colors.textSecondary;
                locationOpt.style = fst::Style().withWidth(std::max(180.0f, bounds.width() * 0.42f));
                fst::Label(ctx, location, locationOpt);
            }

            fst::ButtonOptions jumpButton;
            jumpButton.style = fst::Style().withWidth(80.0f);
            jumpButton.disabled = !canJump;
            const bool jumpClicked = fst::Button(ctx, "Przejdz", jumpButton);
            fst::EndHorizontal(ctx);

            fst::LabelOptions messageOpt;
            messageOpt.color = err.isError ? theme.colors.text : theme.colors.textSecondary;
            fst::Label(ctx, message, messageOpt);

            if (jumpClicked && canJump) {
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
            fst::Separator(ctx);
        }
    } else {
        fst::LabelSecondary(ctx, "Brak bledow kompilatora.");
    }

    fst::LabelSecondary(ctx, "Wyjscie kompilacji:");
    fst::TextAreaOptions outputOptions;
    outputOptions.readonly = true;
    outputOptions.wordWrap = false;
    outputOptions.showLineNumbers = true;
    outputOptions.style = fst::Style().withWidth(std::max(100.0f, bounds.width() - 8.0f));
    const float reservedTop = errorList.empty() ? 130.0f : 250.0f;
    outputOptions.height = std::max(140.0f, bounds.height() - reservedTop);
    fst::TextArea(ctx, "build_output", compilationOutput, outputOptions);

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin
