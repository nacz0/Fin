#include "SearchLogic.h"

void FindNext(TextEditor& editor, const std::string& query) {
    if (query.empty()) return;

    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();
    int totalLines = (int)lines.size();

    if (pos.mLine < totalLines) {
        const std::string& currentLine = lines[pos.mLine];
        size_t found = currentLine.find(query, pos.mColumn);
        
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = {pos.mLine, (int)found};
            TextEditor::Coordinates sEnd = {pos.mLine, (int)(found + query.length())};
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd);
            return;
        }
    }

    for (int i = pos.mLine + 1; i < totalLines; ++i) {
        size_t found = lines[i].find(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd);
            return;
        }
    }

    for (int i = 0; i <= pos.mLine; ++i) {
        size_t found = std::string::npos;
        
        if (i == pos.mLine) {
            std::string sub = lines[i].substr(0, pos.mColumn);
            found = sub.find(query);
        } else {
            found = lines[i].find(query);
        }

        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sEnd);
            return;
        }
    }
}

void FindPrev(TextEditor& editor, const std::string& query) {
    if (query.empty()) return;

    auto pos = editor.GetCursorPosition();
    auto& lines = editor.GetTextLines();
    int totalLines = (int)lines.size();

    if (pos.mLine < totalLines && pos.mColumn > 0) {
        size_t searchLimit = pos.mColumn; 
        if (searchLimit > 0) searchLimit--; 

        size_t found = lines[pos.mLine].rfind(query, searchLimit);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { pos.mLine, (int)found };
            TextEditor::Coordinates sEnd = { pos.mLine, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
            return;
        }
    }

    for (int i = pos.mLine - 1; i >= 0; --i) {
        size_t found = lines[i].rfind(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
            return;
        }
    }

    for (int i = totalLines - 1; i > pos.mLine; --i) {
        size_t found = lines[i].rfind(query);
        if (found != std::string::npos) {
            TextEditor::Coordinates sStart = { i, (int)found };
            TextEditor::Coordinates sEnd = { i, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
            return;
        }
    }

    if (pos.mLine < totalLines) {
        size_t found = lines[pos.mLine].rfind(query);
        if (found != std::string::npos && found >= (size_t)pos.mColumn) {
            TextEditor::Coordinates sStart = { pos.mLine, (int)found };
            TextEditor::Coordinates sEnd = { pos.mLine, (int)(found + query.length()) };
            editor.SetSelection(sStart, sEnd);
            editor.SetCursorPosition(sStart);
        }
    }
}

void UpdateSearchInfo(TextEditor& editor, const std::string& query, int& outCount, int& outIndex) {
    outCount = 0;
    outIndex = 0;
    if (query.empty()) return;

    auto& lines = editor.GetTextLines();
    auto cursorPos = editor.GetCursorPosition();

    for (int i = 0; i < (int)lines.size(); ++i) {
        size_t found = lines[i].find(query);
        while (found != std::string::npos) {
            outCount++;

            if (i == cursorPos.mLine) {
                int start = (int)found;
                int end = (int)(found + query.length());
                if (cursorPos.mColumn >= start && cursorPos.mColumn <= end) {
                    outIndex = outCount;
                }
            }

            found = lines[i].find(query, found + 1);
        }
    }
}
