#include "App/FinCompletionLocal.h"

#include "App/FinHelpers.h"

#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace fin {

std::vector<LSPCompletionItem> CollectLocalCompletions(
    const DocumentTab& tab,
    const fst::TextPosition& cursor) {
    std::vector<LSPCompletionItem> out;

    const std::string fullText = tab.editor.getText();
    size_t offset = offsetFromPosition(fullText, cursor);
    if (offset > fullText.size()) {
        offset = fullText.size();
    }

    size_t lineStart = offset;
    while (lineStart > 0 && fullText[lineStart - 1] != '\n') {
        --lineStart;
    }
    const std::string beforeCursor = fullText.substr(lineStart, offset - lineStart);

    bool inStdScope = false;
    std::string typedPrefix;

    const size_t scopePos = beforeCursor.rfind("::");
    if (scopePos != std::string::npos) {
        size_t idEnd = scopePos;
        size_t idStart = idEnd;
        while (idStart > 0) {
            const char ch = beforeCursor[idStart - 1];
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch) || ch == '_') {
                --idStart;
                continue;
            }
            break;
        }

        const std::string qualifier = beforeCursor.substr(idStart, idEnd - idStart);
        if (qualifier == "std") {
            inStdScope = true;
            typedPrefix = beforeCursor.substr(scopePos + 2);
        }
    }

    if (!inStdScope) {
        size_t idStart = beforeCursor.size();
        while (idStart > 0) {
            const char ch = beforeCursor[idStart - 1];
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch) || ch == '_') {
                --idStart;
                continue;
            }
            break;
        }
        typedPrefix = beforeCursor.substr(idStart);
    }

    if (!inStdScope && typedPrefix.size() < 1) {
        return out;
    }

    static const std::vector<std::string> kCppKeywords = {
        "alignas", "alignof", "auto", "bool", "break", "case", "catch", "char", "class", "const",
        "constexpr", "continue", "default", "delete", "do", "double", "else", "enum", "explicit",
        "extern", "false", "float", "for", "friend", "if", "inline", "int", "long", "namespace",
        "new", "noexcept", "nullptr", "operator", "private", "protected", "public", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw",
        "true", "try", "typedef", "typename", "union", "unsigned", "using", "virtual", "void",
        "volatile", "while"};

    static const std::vector<std::string> kStdSymbols = {
        "array", "begin", "cout", "cerr", "cin", "clog", "deque", "endl", "end", "getline", "list",
        "make_pair", "make_shared", "make_unique", "map", "max", "min", "move", "optional", "pair",
        "set", "shared_ptr", "sort", "span", "stack", "string", "string_view", "swap", "tuple",
        "unordered_map", "unordered_set", "unique_ptr", "vector"};

    std::unordered_set<std::string> seen;
    std::vector<std::string> candidates;
    candidates.reserve(256);

    auto pushCandidate = [&](const std::string& value) {
        if (value.empty() || seen.count(value) > 0) {
            return;
        }
        seen.insert(value);
        candidates.push_back(value);
    };

    if (inStdScope) {
        for (const auto& s : kStdSymbols) {
            pushCandidate(s);
        }
    } else {
        for (const auto& s : kCppKeywords) {
            pushCandidate(s);
        }
    }

    std::string token;
    token.reserve(32);
    for (char ch : fullText) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_') {
            token.push_back(ch);
        } else {
            if (token.size() >= 3) {
                pushCandidate(token);
            }
            token.clear();
        }
    }
    if (token.size() >= 3) {
        pushCandidate(token);
    }

    for (const std::string& candidate : candidates) {
        if (!typedPrefix.empty() && candidate.rfind(typedPrefix, 0) != 0) {
            continue;
        }
        if (!typedPrefix.empty() && candidate == typedPrefix) {
            continue;
        }

        LSPCompletionItem item;
        item.label = candidate;
        item.detail = "local";
        item.insertText = typedPrefix.empty() ? candidate : candidate.substr(typedPrefix.size());
        if (item.insertText.empty()) {
            item.insertText = candidate;
        }

        out.push_back(item);
        if (out.size() >= 40) {
            break;
        }
    }

    return out;
}

} // namespace fin
