#include "App/Panels/ConsolePanel.h"

#include "fastener/fastener.h"

#include <algorithm>
#include <unordered_set>

namespace fin {

void RenderConsolePanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    const std::vector<ParsedError>& errorList,
    std::string& compilationOutput,
    const OpenDocumentFn& openDocument,
    const ClampActiveTabFn& clampActiveTab) {
    if (!fst::BeginDockableWindow(ctx, fst::i18n("window.console"))) {
        return;
    }

    const fst::Rect bounds = ctx.layout().currentBounds();
    ctx.layout().beginContainer(bounds);
    const fst::Theme& theme = ctx.theme();

    std::vector<const ParsedError*> uniqueErrors;
    uniqueErrors.reserve(errorList.size());
    std::unordered_set<std::string> seenErrors;
    for (const ParsedError& err : errorList) {
        const std::string message = err.message.empty() ? err.fullMessage : err.message;
        const std::string key = err.filename + "|" + std::to_string(err.line) + "|" + std::to_string(err.col) +
                                "|" + (err.isError ? "E" : "W") + "|" + message;
        if (seenErrors.insert(key).second) {
            uniqueErrors.push_back(&err);
        }
    }

    int errorCount = 0;
    int warningCount = 0;
    for (const ParsedError* err : uniqueErrors) {
        if (err->isError) {
            ++errorCount;
        } else {
            ++warningCount;
        }
    }

    fst::Label(ctx, fst::i18n("console.compilation_result"));
    fst::BeginHorizontal(ctx, 14.0f);
    {
        fst::LabelOptions errorsOpt;
        errorsOpt.color = errorCount > 0 ? theme.colors.error : theme.colors.success;
        fst::Label(ctx, fst::i18n("console.errors", {std::to_string(errorCount)}), errorsOpt);

        fst::LabelOptions warningsOpt;
        warningsOpt.color = warningCount > 0 ? theme.colors.warning : theme.colors.textSecondary;
        fst::Label(ctx, fst::i18n("console.warnings", {std::to_string(warningCount)}), warningsOpt);
    }
    fst::EndHorizontal(ctx);
    fst::Separator(ctx);

    if (!uniqueErrors.empty()) {
        fst::Label(ctx, fst::i18n("console.compiler_issues"));
        const int duplicateCount = static_cast<int>(errorList.size() - uniqueErrors.size());
        if (duplicateCount > 0) {
            fst::LabelSecondary(ctx, fst::i18n("console.duplicates_skipped", {std::to_string(duplicateCount)}));
        }
        for (const ParsedError* errPtr : uniqueErrors) {
            const ParsedError& err = *errPtr;
            const bool canJump = !err.filename.empty() && err.line > 0;
            const std::string location = canJump
                ? (err.filename + ":" + std::to_string(err.line) + ":" + std::to_string(err.col))
                : fst::i18n("console.no_location");
            std::string message = err.message.empty() ? err.fullMessage : err.message;
            if (message.rfind("error: ", 0) == 0) {
                message.erase(0, 7);
            } else if (message.rfind("fatal error: ", 0) == 0) {
                message.erase(0, 13);
            }
            const std::string clickableLine = location + "  " + message;

            fst::BeginHorizontal(ctx, 10.0f);
            {
                fst::LabelOptions levelOpt;
                levelOpt.color = err.isError ? theme.colors.error : theme.colors.warning;
                levelOpt.style = fst::Style().withWidth(62.0f);
                fst::Label(ctx, err.isError ? fst::i18n("console.level.error") : fst::i18n("console.level.warn"), levelOpt);

                bool selected = false;
                fst::SelectableOptions selectableOpt;
                selectableOpt.disabled = !canJump;
                if (fst::Selectable(ctx, clickableLine, selected, selectableOpt) && canJump) {
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
            fst::EndHorizontal(ctx);

            if (!canJump) {
                fst::LabelSecondary(ctx, fst::i18n("console.no_jump_location"));
            }
            fst::Separator(ctx);
        }
    } else {
        fst::LabelSecondary(ctx, fst::i18n("console.no_compiler_errors"));
    }

    fst::LabelSecondary(ctx, fst::i18n("console.compilation_output"));
    fst::TextAreaOptions outputOptions;
    outputOptions.readonly = true;
    outputOptions.wordWrap = false;
    outputOptions.showLineNumbers = true;
    outputOptions.style = fst::Style().withWidth(std::max(100.0f, bounds.width() - 8.0f));
    const float reservedTop = uniqueErrors.empty() ? 130.0f : 250.0f;
    outputOptions.height = std::max(140.0f, bounds.height() - reservedTop);
    fst::TextArea(ctx, "build_output", compilationOutput, outputOptions);

    ctx.layout().endContainer();
    fst::EndDockableWindow(ctx);
}

} // namespace fin
