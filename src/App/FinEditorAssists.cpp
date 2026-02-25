#include "App/FinEditorAssists.h"

#include "App/FinHelpers.h"

#include <cctype>
#include <string>
#include <vector>

namespace fin {

namespace {

char matchingClosing(char ch) {
    switch (ch) {
        case '(':
            return ')';
        case '[':
            return ']';
        case '{':
            return '}';
        case '"':
            return '"';
        case '\'':
            return '\'';
        default:
            return '\0';
    }
}

bool isClosingChar(char ch) {
    return ch == ')' || ch == ']' || ch == '}' || ch == '"' || ch == '\'';
}

} // namespace

void ApplyEditorAssists(
    const AppConfig& config,
    fst::InputState& input,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    bool assistHasPreState,
    int assistPreTab,
    const std::string& assistPreText,
    const fst::TextPosition& assistPreCursor,
    bool completionVisible,
    const std::function<void()>& clampActiveTab,
    const std::function<void()>& closeCompletionPopup) {
    clampActiveTab();
    if (activeTab < 0 || activeTab >= static_cast<int>(docs.size())) {
        return;
    }
    if (input.modifiers().ctrl || input.modifiers().alt || input.modifiers().super) {
        return;
    }
    if (!config.autoClosingBrackets && !config.smartIndentEnabled) {
        return;
    }

    DocumentTab& tab = *docs[activeTab];
    std::string text = tab.editor.getText();
    size_t cursorOffset = offsetFromPosition(text, tab.editor.cursor());
    if (cursorOffset > text.size()) {
        cursorOffset = text.size();
    }

    bool changed = false;
    const std::string& typed = input.textInput();

    if (config.autoClosingBrackets &&
        input.isKeyPressed(fst::Key::Backspace) &&
        assistHasPreState &&
        assistPreTab == activeTab) {
        size_t oldCursorOffset = offsetFromPosition(assistPreText, assistPreCursor);
        if (oldCursorOffset > assistPreText.size()) {
            oldCursorOffset = assistPreText.size();
        }

        if (oldCursorOffset > 0 &&
            oldCursorOffset < assistPreText.size() &&
            text.size() + 1 == assistPreText.size() &&
            cursorOffset + 1 == oldCursorOffset) {
            const char oldLeft = assistPreText[oldCursorOffset - 1];
            const char oldRight = assistPreText[oldCursorOffset];
            const char expectedRight = matchingClosing(oldLeft);
            if (expectedRight != '\0' && expectedRight == oldRight) {
                std::string expectedText = assistPreText;
                expectedText.erase(oldCursorOffset - 1, 1);
                if (expectedText == text &&
                    cursorOffset < text.size() &&
                    text[cursorOffset] == oldRight) {
                    text.erase(cursorOffset, 1);
                    changed = true;
                }
            }
        }
    }

    if (config.autoClosingBrackets && typed.size() == 1 && cursorOffset > 0) {
        const char typedChar = typed[0];

        if (isClosingChar(typedChar) &&
            cursorOffset < text.size() &&
            text[cursorOffset - 1] == typedChar &&
            text[cursorOffset] == typedChar) {
            text.erase(cursorOffset - 1, 1);
            changed = true;
        } else {
            const char closing = matchingClosing(typedChar);
            if (closing != '\0' && text[cursorOffset - 1] == typedChar) {
                const char next = (cursorOffset < text.size()) ? text[cursorOffset] : '\0';
                bool shouldInsertClosing = true;

                if (typedChar == '"' || typedChar == '\'') {
                    const char prev = (cursorOffset >= 2) ? text[cursorOffset - 2] : '\0';
                    const unsigned char nextUch = static_cast<unsigned char>(next);
                    if (prev == '\\' || next == typedChar || std::isalnum(nextUch) || next == '_') {
                        shouldInsertClosing = false;
                    }
                } else if (next != '\0') {
                    static const std::string kStopChars = " \t\r\n)]},;:";
                    if (kStopChars.find(next) == std::string::npos) {
                        shouldInsertClosing = false;
                    }
                }

                if (shouldInsertClosing) {
                    text.insert(cursorOffset, 1, closing);
                    changed = true;
                }
            }
        }
    }

    if (config.smartIndentEnabled && input.isKeyPressed(fst::Key::Enter)) {
        if (cursorOffset >= 2 && cursorOffset <= text.size() &&
            text[cursorOffset - 1] == '\n' &&
            text[cursorOffset - 2] == '{' &&
            cursorOffset < text.size() &&
            text[cursorOffset] == '}') {
            size_t lineStart = cursorOffset;
            while (lineStart > 0 && text[lineStart - 1] != '\n') {
                --lineStart;
            }

            size_t indentEnd = lineStart;
            while (indentEnd < text.size() && (text[indentEnd] == ' ' || text[indentEnd] == '\t')) {
                ++indentEnd;
            }

            const std::string baseIndent = text.substr(lineStart, indentEnd - lineStart);
            const std::string indentUnit = (baseIndent.find('\t') != std::string::npos) ? "\t" : "    ";
            const std::string insertion = baseIndent + indentUnit + "\n";

            text.insert(cursorOffset, insertion);
            cursorOffset += baseIndent.size() + indentUnit.size();
            changed = true;
        }
    }

    if (!changed) {
        return;
    }

    tab.editor.setText(text);
    tab.editor.setCursor(positionFromOffset(text, cursorOffset));
    tab.dirty = (text != tab.savedText);
    if (completionVisible) {
        closeCompletionPopup();
    }
}

} // namespace fin
