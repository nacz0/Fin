#include "App/FinHelpers.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace fin {

namespace {

struct CppSyntaxPalette {
    fst::Color keyword;
    fst::Color type;
    fst::Color number;
    fst::Color stringLiteral;
    fst::Color comment;
    fst::Color preprocessor;
    fst::Color function;
    fst::Color punctuation;
};

struct ScrollablePanelState {
    float scrollOffsetY = 0.0f;
    float contentHeight = 0.0f;
    bool draggingScrollbar = false;
    float dragStartMouseY = 0.0f;
    float dragStartScroll = 0.0f;
};

struct ScrollbarGeometry {
    fst::Rect track;
    fst::Rect thumb;
    float maxScroll = 0.0f;
    bool visible = false;
};

std::unordered_map<fst::WidgetId, ScrollablePanelState> g_scrollablePanelStates;

std::string ScrollbarWidgetLabel(std::string_view id) {
    std::string label(id);
    label += "_scrollbar";
    return label;
}

ScrollbarGeometry CalculateScrollbarGeometry(
    const fst::Rect& bounds,
    float contentHeight,
    float scrollOffsetY,
    const fst::Theme& theme) {
    ScrollbarGeometry geometry;
    const float viewHeight = std::max(1.0f, bounds.height());
    const float normalizedContentHeight = std::max(viewHeight, contentHeight);
    geometry.maxScroll = std::max(0.0f, normalizedContentHeight - viewHeight);
    if (geometry.maxScroll <= 0.0f) {
        return geometry;
    }

    const float scrollbarWidth = std::max(8.0f, theme.metrics.scrollbarWidth);
    geometry.track = fst::Rect(bounds.right() - scrollbarWidth, bounds.y(), scrollbarWidth, bounds.height());

    const float thumbHeight =
        std::clamp((viewHeight / normalizedContentHeight) * geometry.track.height(), 20.0f, geometry.track.height());
    const float thumbTravel = std::max(1.0f, geometry.track.height() - thumbHeight);
    const float t = std::clamp(scrollOffsetY / geometry.maxScroll, 0.0f, 1.0f);
    const float thumbY = geometry.track.y() + t * thumbTravel;
    geometry.thumb = fst::Rect(
        geometry.track.x() + 2.0f,
        thumbY,
        std::max(2.0f, geometry.track.width() - 4.0f),
        thumbHeight);
    geometry.visible = true;
    return geometry;
}

} // namespace

void beginScrollablePanelContent(fst::Context& ctx, std::string_view id, const fst::Rect& bounds) {
    const fst::WidgetId regionId = ctx.makeId(id);
    ScrollablePanelState& state = g_scrollablePanelStates[regionId];
    auto& input = ctx.input();
    const fst::Theme& theme = ctx.theme();
    const fst::Vec2 mousePos = input.mousePos();
    const bool hovered = bounds.contains(mousePos) && !ctx.isOccluded(mousePos);
    const std::string scrollbarLabel = ScrollbarWidgetLabel(id);
    const fst::WidgetId scrollbarId = ctx.makeId(scrollbarLabel);

    ScrollbarGeometry geometry = CalculateScrollbarGeometry(bounds, state.contentHeight, state.scrollOffsetY, theme);
    if (!geometry.visible) {
        state.draggingScrollbar = false;
        if (ctx.isCapturedBy(scrollbarId)) {
            ctx.clearActiveWidget();
        }
    } else {
        if (hovered && input.scrollDelta().y != 0.0f) {
            const float lineHeight = ctx.font() ? ctx.font()->lineHeight() : 14.0f;
            const float scrollStep = std::max(24.0f, lineHeight * 2.5f);
            state.scrollOffsetY -= input.scrollDelta().y * scrollStep;
        }

        if (input.isMousePressed(fst::MouseButton::Left) &&
            geometry.track.contains(mousePos) &&
            !ctx.isOccluded(mousePos)) {
            state.draggingScrollbar = true;
            state.dragStartMouseY = mousePos.y;
            state.dragStartScroll = state.scrollOffsetY;
            ctx.setActiveWidget(scrollbarId);
            input.consumeMouse();

            if (!geometry.thumb.contains(mousePos)) {
                const float thumbTravel = std::max(1.0f, geometry.track.height() - geometry.thumb.height());
                const float clickT = std::clamp(
                    (mousePos.y - geometry.track.y() - geometry.thumb.height() * 0.5f) / thumbTravel,
                    0.0f,
                    1.0f);
                state.scrollOffsetY = clickT * geometry.maxScroll;
                state.dragStartScroll = state.scrollOffsetY;
            }
        }

        if (state.draggingScrollbar) {
            if (input.isMouseDown(fst::MouseButton::Left)) {
                const float thumbTravel = std::max(1.0f, geometry.track.height() - geometry.thumb.height());
                const float scrollPerPixel = geometry.maxScroll / thumbTravel;
                state.scrollOffsetY = state.dragStartScroll + (mousePos.y - state.dragStartMouseY) * scrollPerPixel;
                ctx.window().setCursor(fst::Cursor::ResizeV);
            } else {
                state.draggingScrollbar = false;
                if (ctx.isCapturedBy(scrollbarId)) {
                    ctx.clearActiveWidget();
                }
            }
        } else if (geometry.track.contains(mousePos) && !ctx.isOccluded(mousePos)) {
            ctx.window().setCursor(fst::Cursor::ResizeV);
        }
    }

    if (geometry.maxScroll > 0.0f) {
        state.scrollOffsetY = std::clamp(state.scrollOffsetY, 0.0f, geometry.maxScroll);
    } else {
        state.scrollOffsetY = 0.0f;
    }

    ctx.layout().beginContainer(bounds);
    ctx.layout().setScroll(0.0f, state.scrollOffsetY);
}

void endScrollablePanelContent(fst::Context& ctx, std::string_view id, const fst::Rect& bounds) {
    const fst::WidgetId regionId = ctx.makeId(id);
    ScrollablePanelState& state = g_scrollablePanelStates[regionId];
    const fst::Theme& theme = ctx.theme();

    const fst::Vec2 cursor = ctx.layout().currentPosition();
    state.contentHeight = std::max(bounds.height(), cursor.y - bounds.y());
    ScrollbarGeometry geometry = CalculateScrollbarGeometry(bounds, state.contentHeight, state.scrollOffsetY, theme);
    if (geometry.maxScroll > 0.0f) {
        state.scrollOffsetY = std::clamp(state.scrollOffsetY, 0.0f, geometry.maxScroll);
        geometry = CalculateScrollbarGeometry(bounds, state.contentHeight, state.scrollOffsetY, theme);

        if (geometry.visible) {
            auto& dl = ctx.drawList();
            const fst::Vec2 mousePos = ctx.input().mousePos();
            const bool hovered = geometry.track.contains(mousePos) && !ctx.isOccluded(mousePos);
            dl.addRectFilled(geometry.track, theme.colors.scrollbarTrack);
            const fst::Color thumbColor = (state.draggingScrollbar || hovered)
                ? theme.colors.scrollbarThumbHover
                : theme.colors.scrollbarThumb;
            dl.addRectFilled(geometry.thumb, thumbColor, geometry.thumb.width() * 0.5f);
        }
    } else {
        state.scrollOffsetY = 0.0f;
    }

    ctx.layout().endContainer();
}

namespace {

bool isIdentifierStart(char ch) {
    const unsigned char c = static_cast<unsigned char>(ch);
    return std::isalpha(c) || ch == '_';
}

bool isIdentifierChar(char ch) {
    const unsigned char c = static_cast<unsigned char>(ch);
    return std::isalnum(c) || ch == '_';
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isCppLikePathInternal(const std::string& pathOrName) {
    static const std::unordered_set<std::string> kExtensions = {
        ".c", ".cc", ".cpp", ".cxx", ".c++", ".h", ".hh", ".hpp", ".hxx", ".h++", ".ipp", ".ixx", ".inl", ".tpp"};

    const std::string ext = toLowerAscii(fs::path(pathOrName).extension().string());
    return kExtensions.count(ext) > 0;
}

float luminance(const fst::Color& color) {
    return 0.2126f * static_cast<float>(color.r) +
           0.7152f * static_cast<float>(color.g) +
           0.0722f * static_cast<float>(color.b);
}

CppSyntaxPalette buildPalette(const fst::Theme& theme) {
    const bool darkBackground = luminance(theme.colors.windowBackground) < 128.0f;
    if (darkBackground) {
        return {
            fst::Color::fromHex(0x82aaff), // keyword
            fst::Color::fromHex(0x4ec9b0), // type
            fst::Color::fromHex(0xb5cea8), // number
            fst::Color::fromHex(0xce9178), // string
            fst::Color::fromHex(0x6a9955), // comment
            fst::Color::fromHex(0xc586c0), // preprocessor
            fst::Color::fromHex(0xdcdcaa), // function
            fst::Color::fromHex(0xd4d4d4)  // punctuation
        };
    }

    return {
        fst::Color::fromHex(0x1d4ed8), // keyword
        fst::Color::fromHex(0x0f766e), // type
        fst::Color::fromHex(0x9a3412), // number
        fst::Color::fromHex(0xb45309), // string
        fst::Color::fromHex(0x15803d), // comment
        fst::Color::fromHex(0x7e22ce), // preprocessor
        fst::Color::fromHex(0x92400e), // function
        fst::Color::fromHex(0x4b5563)  // punctuation
    };
}

std::vector<fst::TextSegment> colorizeCppLine(const std::string& text, const CppSyntaxPalette& palette) {
    static const std::unordered_set<std::string> kKeywords = {
        "alignas",     "alignof",      "asm",          "auto",         "break",        "case",
        "catch",       "class",        "const",        "consteval",    "constexpr",    "constinit",
        "continue",    "co_await",     "co_return",    "co_yield",     "decltype",     "default",
        "delete",      "do",           "else",         "enum",         "explicit",     "export",
        "extern",      "false",        "final",        "for",          "friend",       "goto",
        "if",          "import",       "inline",       "module",       "mutable",      "namespace",
        "new",         "noexcept",     "nullptr",      "operator",     "override",     "private",
        "protected",   "public",       "register",     "requires",     "return",       "sizeof",
        "static",      "static_assert","struct",       "switch",       "template",     "this",
        "thread_local","throw",        "true",         "try",          "typedef",      "typename",
        "union",       "using",        "virtual",      "volatile",     "while"};

    static const std::unordered_set<std::string> kTypeWords = {
        "bool",     "char",      "char8_t",   "char16_t", "char32_t", "double",     "float",
        "int",      "long",      "short",     "signed",   "unsigned", "void",       "wchar_t",
        "size_t",   "ptrdiff_t", "uint8_t",   "uint16_t", "uint32_t", "uint64_t",   "int8_t",
        "int16_t",  "int32_t",   "int64_t",   "std",      "string"};

    static const std::unordered_set<std::string> kControlWords = {
        "if", "for", "while", "switch", "catch", "return", "sizeof", "decltype"};
    static const std::unordered_set<std::string> kTypeIntroducers = {
        "class", "struct", "typename", "enum", "union", "using"};

    std::vector<fst::TextSegment> segments;
    const int lineLength = static_cast<int>(text.size());
    if (lineLength == 0) {
        return segments;
    }

    const size_t firstCode = text.find_first_not_of(" \t");
    if (firstCode != std::string::npos && text[firstCode] == '#') {
        segments.push_back({static_cast<int>(firstCode), lineLength, palette.preprocessor});
        return segments;
    }

    int i = 0;
    bool expectTypeName = false;
    while (i < lineLength) {
        const char ch = text[i];

        if (ch == '/' && i + 1 < lineLength && text[i + 1] == '/') {
            segments.push_back({i, lineLength, palette.comment});
            break;
        }

        if (ch == '/' && i + 1 < lineLength && text[i + 1] == '*') {
            int end = lineLength;
            const size_t close = text.find("*/", static_cast<size_t>(i + 2));
            if (close != std::string::npos) {
                end = static_cast<int>(close + 2);
            }
            segments.push_back({i, end, palette.comment});
            i = end;
            continue;
        }

        if (ch == '"' || ch == '\'') {
            const char quote = ch;
            int end = i + 1;
            while (end < lineLength) {
                if (text[end] == '\\' && end + 1 < lineLength) {
                    end += 2;
                    continue;
                }
                if (text[end] == quote) {
                    ++end;
                    break;
                }
                ++end;
            }
            segments.push_back({i, std::min(end, lineLength), palette.stringLiteral});
            i = end;
            continue;
        }

        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isdigit(uch) || (ch == '.' && i + 1 < lineLength && std::isdigit(static_cast<unsigned char>(text[i + 1])))) {
            int end = i;
            if (ch == '0' && i + 1 < lineLength && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
                end += 2;
                while (end < lineLength && std::isxdigit(static_cast<unsigned char>(text[end]))) {
                    ++end;
                }
            } else {
                bool seenDot = (ch == '.');
                while (end < lineLength) {
                    const char n = text[end];
                    const unsigned char un = static_cast<unsigned char>(n);
                    if (std::isdigit(un)) {
                        ++end;
                        continue;
                    }
                    if (n == '.' && !seenDot) {
                        seenDot = true;
                        ++end;
                        continue;
                    }
                    if ((n == 'e' || n == 'E') && end + 1 < lineLength) {
                        int expPos = end + 1;
                        if (text[expPos] == '+' || text[expPos] == '-') {
                            ++expPos;
                        }
                        if (expPos < lineLength && std::isdigit(static_cast<unsigned char>(text[expPos]))) {
                            end = expPos + 1;
                            while (end < lineLength && std::isdigit(static_cast<unsigned char>(text[end]))) {
                                ++end;
                            }
                            continue;
                        }
                    }
                    break;
                }
            }
            while (end < lineLength && std::isalpha(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
            segments.push_back({i, end, palette.number});
            i = end;
            continue;
        }

        if (isIdentifierStart(ch)) {
            int end = i + 1;
            while (end < lineLength && isIdentifierChar(text[end])) {
                ++end;
            }

            const std::string word = text.substr(static_cast<size_t>(i), static_cast<size_t>(end - i));
            if (kTypeWords.count(word) > 0) {
                segments.push_back({i, end, palette.type});
                expectTypeName = false;
            } else if (kKeywords.count(word) > 0) {
                segments.push_back({i, end, palette.keyword});
                expectTypeName = kTypeIntroducers.count(word) > 0;
            } else {
                if (expectTypeName || std::isupper(static_cast<unsigned char>(word[0])) != 0) {
                    segments.push_back({i, end, palette.type});
                    expectTypeName = false;
                } else {
                    int probe = end;
                    while (probe < lineLength && (text[probe] == ' ' || text[probe] == '\t')) {
                        ++probe;
                    }
                    if (probe < lineLength && text[probe] == '(' && kControlWords.count(word) == 0) {
                        segments.push_back({i, end, palette.function});
                    } else if (probe < lineLength && text[probe] == '<') {
                        segments.push_back({i, end, palette.function});
                    }
                }
            }
            i = end;
            continue;
        }

        if (ch == ':' && i + 1 < lineLength && text[i + 1] == ':') {
            segments.push_back({i, i + 2, palette.punctuation});
            i += 2;
            continue;
        }

        if (ch == '-' && i + 1 < lineLength && text[i + 1] == '>') {
            segments.push_back({i, i + 2, palette.punctuation});
            i += 2;
            continue;
        }

        if (ch == '<' || ch == '>' || ch == '(' || ch == ')' ||
            ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == ',' || ch == '*' || ch == '&' || ch == '=') {
            segments.push_back({i, i + 1, palette.punctuation});
            ++i;
            continue;
        }

        ++i;
    }

    return segments;
}

} // namespace

void trimBuffer(std::string& text, size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return;
    }
    text.erase(0, text.size() - maxBytes);
}

static fst::Theme retroTheme() {
    fst::Theme theme = fst::Theme::dark();
    theme.colors.windowBackground = fst::Color::fromHex(0x0b1028);
    theme.colors.panelBackground = fst::Color::fromHex(0x141c3d);
    theme.colors.primary = fst::Color::fromHex(0x2459d4);
    theme.colors.primaryHover = fst::Color::fromHex(0x2d6af8);
    theme.colors.primaryActive = fst::Color::fromHex(0x1d47aa);
    theme.colors.text = fst::Color::fromHex(0xe8f0ff);
    theme.colors.textSecondary = fst::Color::fromHex(0x9fb4df);
    theme.colors.buttonBackground = fst::Color::fromHex(0x22335f);
    theme.colors.buttonHover = fst::Color::fromHex(0x29427d);
    theme.colors.buttonActive = fst::Color::fromHex(0x1a2d56);
    theme.colors.border = fst::Color::fromHex(0x2f4d8d);
    theme.colors.dockTabActive = fst::Color::fromHex(0x2d6af8);
    theme.colors.dockTabInactive = fst::Color::fromHex(0x172445);
    theme.colors.dockTabHover = fst::Color::fromHex(0x223b70);
    return theme;
}

void applyTheme(fst::Context& ctx, int themeId) {
    if (themeId == 1) {
        ctx.setTheme(fst::Theme::light());
        return;
    }
    if (themeId == 2) {
        ctx.setTheme(retroTheme());
        return;
    }
    ctx.setTheme(fst::Theme::dark());
}

bool isCppLikePath(const std::string& pathOrName) {
    return isCppLikePathInternal(pathOrName);
}

void applyCppSyntaxHighlighting(fst::TextEditor& editor, const std::string& pathOrName, const fst::Theme& theme) {
    if (!isCppLikePath(pathOrName)) {
        editor.setStyleProvider({});
        return;
    }

    const CppSyntaxPalette palette = buildPalette(theme);
    editor.setStyleProvider([palette](int, const std::string& lineText) {
        return colorizeCppLine(lineText, palette);
    });
}

std::vector<fst::TextSegment> colorizeCppSnippet(const std::string& text, const fst::Theme& theme) {
    return colorizeCppLine(text, buildPalette(theme));
}

std::string normalizePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string normalized = fs::path(path).lexically_normal().string();
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

namespace {

int hexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::string decodeUriEscapes(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '%' && i + 2 < value.size()) {
            const int hi = hexDigitValue(value[i + 1]);
            const int lo = hexDigitValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(ch);
    }
    return decoded;
}

} // namespace

std::string uriToPath(const std::string& uri) {
    std::string path = uri;
    if (path.rfind("file:///", 0) == 0) {
        path = path.substr(8);
    } else if (path.rfind("file://", 0) == 0) {
        path = path.substr(7);
    }

    path = decodeUriEscapes(path);

#ifdef _WIN32
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') {
        path.erase(path.begin());
    }
#endif

    std::replace(path.begin(), path.end(), '/', '\\');
    return normalizePath(path);
}

std::vector<ExplorerEntry> listEntries(const fs::path& root) {
    std::vector<ExplorerEntry> out;
    std::error_code ec;
    fs::directory_iterator it(root, ec);
    if (ec) {
        return out;
    }

    for (const fs::directory_entry& entry : it) {
        std::error_code typeEc;
        const bool isDir = entry.is_directory(typeEc);
        if (typeEc) {
            continue;
        }
        out.push_back({entry.path(), entry.path().filename().string(), isDir});
    }

    std::sort(out.begin(), out.end(), [](const ExplorerEntry& a, const ExplorerEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });

    return out;
}

size_t offsetFromPosition(const std::string& text, const fst::TextPosition& pos) {
    size_t i = 0;
    int line = 0;

    while (i < text.size() && line < pos.line) {
        if (text[i] == '\n') {
            line++;
        }
        i++;
    }

    size_t lineStart = i;
    size_t lineLen = 0;
    while (lineStart + lineLen < text.size() && text[lineStart + lineLen] != '\n') {
        lineLen++;
    }

    size_t col = std::min(static_cast<size_t>(std::max(0, pos.column)), lineLen);
    return lineStart + col;
}

fst::TextPosition positionFromOffset(const std::string& text, size_t offset) {
    fst::TextPosition pos{0, 0};
    const size_t clampedOffset = std::min(offset, text.size());
    for (size_t i = 0; i < clampedOffset; ++i) {
        if (text[i] == '\n') {
            ++pos.line;
            pos.column = 0;
        } else {
            ++pos.column;
        }
    }
    return pos;
}

std::string insertAtPosition(
    const std::string& text,
    const fst::TextPosition& pos,
    const std::string& insertion,
    fst::TextPosition& outCursor) {
    size_t offset = offsetFromPosition(text, pos);
    std::string result = text;
    result.insert(offset, insertion);

    outCursor = pos;
    for (char ch : insertion) {
        if (ch == '\n') {
            outCursor.line++;
            outCursor.column = 0;
        } else {
            outCursor.column++;
        }
    }

    return result;
}

} // namespace fin
