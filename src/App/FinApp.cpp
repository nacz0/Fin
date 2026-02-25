#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fastener/fastener.h"

#include "App/FinApp.h"
#include "App/FinHelpers.h"
#include "App/FinI18n.h"
#include "App/FinTypes.h"
#include "App/Panels/ConsolePanel.h"
#include "App/Panels/EditorPanel.h"
#include "App/Panels/ExplorerPanel.h"
#include "App/Panels/LspDiagnosticsPanel.h"
#include "App/Panels/PersonalizationPanel.h"
#include "App/Panels/SettingsPanel.h"
#include "App/Panels/TerminalPanel.h"

#include "Core/AppConfig.h"
#include "Core/Compiler.h"
#include "Core/ConfigManager.h"
#include "Core/FileManager.h"
#include "Core/LSPClient.h"
#include "Core/Terminal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

using namespace fin;

int RunFinApp() {
    AppConfig config = LoadConfig();
    config.language = NormalizeLocale(config.language);
    InitializeI18n(config.language);

    fst::WindowConfig windowConfig;
    windowConfig.title = "Fin - Fast IDE (Fastener)";
    windowConfig.width = std::max(800, config.windowWidth);
    windowConfig.height = std::max(500, config.windowHeight);
    windowConfig.vsync = true;
    windowConfig.msaaSamples = 8;

    fst::Window window(windowConfig);
    if (!window.isOpen()) {
        return 1;
    }

    fst::Context ctx;
    applyTheme(ctx, config.theme);

    if (!ctx.loadFont("C:/Windows/Fonts/consola.ttf", 15.0f)) {
        ctx.loadFont("C:/Windows/Fonts/arial.ttf", 15.0f);
    }

    Terminal terminal;
    terminal.Start();

    LSPClient lsp;
    bool lspActive = false;
    std::mutex lspMutex;
    std::map<std::string, std::vector<LSPDiagnostic>> pendingDiagnostics;
    std::vector<LSPCompletionItem> pendingCompletions;
    bool pendingCompletionReady = false;
    int completionRequestToken = 0;

    std::vector<std::unique_ptr<DocumentTab>> docs;
    int activeTab = -1;
    int tabCounter = 1;

    float textScale = std::clamp(config.zoom, 0.5f, 3.0f);
    bool layoutInitialized = false;
    bool showSettingsWindow = config.showSettingsWindow;
    bool showExplorerTab = true;
    bool showEditorTab = true;
    bool showConsoleTab = true;
    bool showLspDiagnosticsTab = true;
    bool showTerminalTab = true;
    bool showPersonalizationTab = false;

    std::string terminalInput;
    std::string terminalHistory;
    std::string statusText = fst::i18n("status.ready");
    std::string compilationOutput = fst::i18n("status.compilation_ready");

    std::vector<ParsedError> errorList;
    std::future<std::string> compilationTask;
    bool isCompiling = false;

    bool completionVisible = false;
    bool completionLoading = false;
    int completionSelected = 0;
    float completionScrollOffset = 0.0f;
    bool completionScrollbarDragging = false;
    float completionScrollbarDragOffset = 0.0f;
    fst::Key completionRepeatKey = fst::Key::Unknown;
    float completionRepeatTimer = 0.0f;
    std::vector<LSPCompletionItem> completionItems;
    int completionOwnerTab = -1;
    std::string completionOwnerDocumentPath;

    fs::path currentPath = fs::exists(config.lastDirectory) ? fs::path(config.lastDirectory) : fs::current_path();

    fst::MenuBar menuBar;
    fst::TabControl tabControl;

    fst::Theme editableTheme = ctx.theme();
    int selectedPersonalizationPreset = std::clamp(config.theme, 0, 2);

    auto clampActiveTab = [&]() {
        if (docs.empty()) {
            activeTab = -1;
            return;
        }
        if (activeTab < 0) {
            activeTab = 0;
        }
        if (activeTab >= static_cast<int>(docs.size())) {
            activeTab = static_cast<int>(docs.size()) - 1;
        }
    };

    auto toLowerExtAscii = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    };

    auto isLspCppPath = [&](const std::string& pathOrName) {
        return toLowerExtAscii(fs::path(pathOrName).extension().string()) == ".cpp";
    };

    auto ensureLspDocumentPath = [&](DocumentTab& tab) -> const std::string& {
        const std::string sourcePath = tab.path.empty() ? tab.name : tab.path;
        if (!isLspCppPath(sourcePath)) {
            tab.lspDocumentPath.clear();
            return tab.lspDocumentPath;
        }

        if (!tab.path.empty()) {
            tab.lspDocumentPath = tab.path;
            return tab.lspDocumentPath;
        }

        if (tab.lspDocumentPath.empty()) {
            std::string ext = fs::path(tab.name).extension().string();
            if (ext.empty()) {
                ext = ".cpp";
            }
            const std::string virtualName = "__fin_unsaved_" + tab.id + ext;
            tab.lspDocumentPath = normalizePath((fs::current_path() / virtualName).string());
        }

        return tab.lspDocumentPath;
    };

    auto sendDidOpenIfPossible = [&](DocumentTab& tab) {
        if (!lspActive) {
            return;
        }

        const std::string& lspDocumentPath = ensureLspDocumentPath(tab);
        if (lspDocumentPath.empty()) {
            tab.lspOpened = false;
            tab.lspTextSnapshot.clear();
            tab.lspDiagnostics.clear();
            return;
        }

        const std::string text = tab.editor.getText();
        lsp.DidOpen(lspDocumentPath, text);
        tab.lspOpened = true;
        tab.lspTextSnapshot = text;
    };

    auto closeCompletionPopup = [&]() {
        ++completionRequestToken;
        completionVisible = false;
        completionLoading = false;
        completionItems.clear();
        completionSelected = 0;
        completionScrollOffset = 0.0f;
        completionScrollbarDragging = false;
        completionScrollbarDragOffset = 0.0f;
        completionRepeatKey = fst::Key::Unknown;
        completionRepeatTimer = 0.0f;
        completionOwnerTab = -1;
        completionOwnerDocumentPath.clear();
        std::lock_guard<std::mutex> lock(lspMutex);
        pendingCompletions.clear();
        pendingCompletionReady = false;
    };

    auto stopLsp = [&]() {
        if (!lspActive) {
            return;
        }

        lsp.Stop();
        lspActive = false;
        closeCompletionPopup();

        for (auto& tab : docs) {
            tab->lspOpened = false;
            tab->lspTextSnapshot.clear();
            tab->lspDiagnostics.clear();
        }

        std::lock_guard<std::mutex> lock(lspMutex);
        pendingDiagnostics.clear();
        pendingCompletions.clear();
        pendingCompletionReady = false;
    };

    auto startLsp = [&]() -> bool {
        if (lspActive) {
            return true;
        }

        if (!lsp.Start()) {
            statusText = fst::i18n("status.lsp_start_failed");
            return false;
        }

        lsp.SetDiagnosticsCallback([&](const std::string& uri, const std::vector<LSPDiagnostic>& diags) {
            const std::string path = uriToPath(uri);
            if (!isLspCppPath(path)) {
                return;
            }
            std::lock_guard<std::mutex> lock(lspMutex);
            pendingDiagnostics[path] = diags;
        });

        lsp.Initialize(fs::current_path().string());
        lspActive = true;

        for (auto& tab : docs) {
            sendDidOpenIfPossible(*tab);
        }

        statusText = fst::i18n("status.lsp_started");
        return true;
    };

    auto closeTab = [&](int index) {
        if (index < 0 || index >= static_cast<int>(docs.size())) {
            return;
        }
        if (completionVisible) {
            closeCompletionPopup();
        }
        docs.erase(docs.begin() + index);
        if (activeTab >= index) {
            activeTab--;
        }
        clampActiveTab();
    };

    auto createTab = [&](const std::string& name, const std::string& path, const std::string& text) {
        auto tab = std::make_unique<DocumentTab>();
        tab->id = "tab_" + std::to_string(tabCounter++);
        tab->name = name;
        tab->path = path.empty() ? std::string() : normalizePath(path);
        tab->lspDocumentPath = tab->path;
        tab->editor.setText(text);
        applyCppSyntaxHighlighting(tab->editor, tab->path.empty() ? tab->name : tab->path, ctx.theme());
        (void)ensureLspDocumentPath(*tab);
        tab->savedText = text;
        docs.push_back(std::move(tab));
        activeTab = static_cast<int>(docs.size()) - 1;
        sendDidOpenIfPossible(*docs.back());
    };

    auto findCompilerPath = [&](const std::string& compilerName) -> std::string {
        const std::string command = "where.exe " + compilerName + " 2>nul";
        std::string output = ExecCommand(command.c_str());
        std::stringstream ss(output);
        std::string firstPath;
        if (std::getline(ss, firstPath)) {
            auto isSpace = [](unsigned char ch) {
                return std::isspace(ch) != 0;
            };

            while (!firstPath.empty() && isSpace(static_cast<unsigned char>(firstPath.back()))) {
                firstPath.pop_back();
            }
            size_t begin = 0;
            while (begin < firstPath.size() && isSpace(static_cast<unsigned char>(firstPath[begin]))) {
                ++begin;
            }
            if (begin > 0) {
                firstPath.erase(0, begin);
            }
        }
        return firstPath;
    };

    auto openDocument = [&](const std::string& path) -> bool {
        if (path.empty()) {
            statusText = fst::i18n("status.enter_file_path");
            return false;
        }

        const std::string normalizedPath = normalizePath(path);
        std::error_code ec;
        if (!fs::exists(normalizedPath, ec) || ec) {
            statusText = fst::i18n("status.file_not_found", {normalizedPath});
            return false;
        }

        if (!fs::is_regular_file(normalizedPath, ec) || ec) {
            statusText = fst::i18n("status.not_a_file", {normalizedPath});
            return false;
        }

        auto toLowerAscii = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        };
        static const std::unordered_set<std::string> kBlockedBinaryExtensions = {
            ".exe", ".dll", ".lib", ".a", ".obj", ".o", ".so", ".dylib", ".pdb", ".ilk", ".class"};
        const std::string ext = toLowerAscii(fs::path(normalizedPath).extension().string());
        if (kBlockedBinaryExtensions.count(ext) > 0) {
            statusText = fst::i18n("status.unsupported_file_type", {normalizedPath});
            return false;
        }

        for (int i = 0; i < static_cast<int>(docs.size()); ++i) {
            if (docs[i]->path == normalizedPath) {
                activeTab = i;
                if (completionVisible) {
                    closeCompletionPopup();
                }
                return true;
            }
        }

        const std::string text = OpenFile(normalizedPath);
        createTab(fs::path(normalizedPath).filename().string(), normalizedPath, text);
        statusText = fst::i18n("status.opened", {normalizedPath});
        if (completionVisible) {
            closeCompletionPopup();
        }
        return true;
    };

    auto saveTabToPath = [&](DocumentTab& tab, const std::string& targetPath) -> bool {
        if (targetPath.empty()) {
            statusText = fst::i18n("status.invalid_file_path");
            return false;
        }

        const std::string normalizedPath = normalizePath(targetPath);
        const std::string previousLspPath = ensureLspDocumentPath(tab);
        tab.path = normalizedPath;
        tab.name = fs::path(normalizedPath).filename().string();
        const std::string& lspDocumentPath = ensureLspDocumentPath(tab);
        applyCppSyntaxHighlighting(tab.editor, tab.path, ctx.theme());

        std::string text = tab.editor.getText();
        SaveFile(tab.path, text);
        tab.savedText = text;
        tab.dirty = false;

        if (lspDocumentPath.empty()) {
            tab.lspOpened = false;
            tab.lspTextSnapshot.clear();
            tab.lspDiagnostics.clear();
        } else if (lspActive) {
            if (!tab.lspOpened || previousLspPath != lspDocumentPath) {
                lsp.DidOpen(lspDocumentPath, text);
                tab.lspOpened = true;
            } else {
                lsp.DidChange(lspDocumentPath, text);
            }
            tab.lspTextSnapshot = text;
        }

        currentPath = fs::path(normalizedPath).parent_path();
        statusText = fst::i18n("status.saved", {tab.path});
        return true;
    };

    auto saveActiveTab = [&]() {
        clampActiveTab();
        if (activeTab < 0) {
            statusText = fst::i18n("status.no_active_tab");
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        if (tab.path.empty()) {
            const std::string chosenPath = ShowSaveFileDialog(tab.name, currentPath.string(), config.language);
            if (chosenPath.empty()) {
                return;
            }
            (void)saveTabToPath(tab, chosenPath);
            return;
        }

        (void)saveTabToPath(tab, tab.path);
    };

    auto runBuild = [&]() {
        clampActiveTab();
        if (isCompiling || activeTab < 0) {
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        if (tab.path.empty()) {
            statusText = fst::i18n("status.save_first");
            return;
        }

        std::string text = tab.editor.getText();
        SaveFile(tab.path, text);
        tab.savedText = text;
        tab.dirty = false;

        const std::string preferredCompiler = config.clangBuildEnabled ? "clang++" : "g++";
        const std::string fallbackCompiler = config.clangBuildEnabled ? "g++" : "clang++";

        std::string compilerPath = findCompilerPath(preferredCompiler);
        bool fallbackUsed = false;
        if (compilerPath.empty()) {
            compilerPath = findCompilerPath(fallbackCompiler);
            fallbackUsed = true;
        }
        if (compilerPath.empty()) {
            statusText = fst::i18n("status.no_compiler");
            return;
        }

        const std::string compilerLabel = fs::path(compilerPath).filename().string();

        isCompiling = true;
        compilationOutput = fst::i18n("status.compiling", {compilerLabel});
        errorList.clear();
        statusText = fallbackUsed
            ? fst::i18n("status.compiling_fallback", {compilerLabel})
            : fst::i18n("status.compiling_using", {compilerLabel});

        const std::string sourcePath = tab.path;
        compilationTask = std::async(std::launch::async, [sourcePath, compilerPath]() {
            fs::path src(sourcePath);
            std::string exePath = (src.parent_path() / (src.stem().string() + ".exe")).string();
            // Avoid starting with a quote: cmd parsing may break for "C:\Program Files\..."
            std::string cmd = "call \"" + compilerPath + "\" -std=c++20 -g \"" + sourcePath +
                              "\" -o \"" + exePath + "\" 2>&1 && call \"" + exePath + "\"";
            return ExecCommand(cmd.c_str());
        });
    };

    auto collectLocalCompletions = [&](DocumentTab& tab, const fst::TextPosition& cursor) {
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
    };

    auto requestCompletionForActive = [&](bool manualRequest) {
        if (!config.autocompleteEnabled && !manualRequest) {
            return;
        }

        clampActiveTab();
        if (activeTab < 0) {
            if (manualRequest) {
                statusText = fst::i18n("status.no_active_tab");
            }
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        const std::string& lspDocumentPath = ensureLspDocumentPath(tab);
        fst::TextPosition cursor = tab.editor.cursor();
        std::vector<LSPCompletionItem> localFallback = collectLocalCompletions(tab, cursor);

        bool canUseLsp = false;
        if (config.autocompleteEnabled && !lspDocumentPath.empty()) {
            if (!lspActive && !startLsp()) {
                canUseLsp = false;
            } else {
                canUseLsp = true;
            }
        }

        if (!canUseLsp || lspDocumentPath.empty()) {
            if (localFallback.empty()) {
                if (manualRequest) {
                    statusText = fst::i18n("status.no_suggestions");
                }
                return;
            }

            completionVisible = true;
            completionLoading = false;
            completionItems = localFallback;
            completionSelected = 0;
            completionScrollOffset = 0.0f;
            completionScrollbarDragging = false;
            completionRepeatKey = fst::Key::Unknown;
            completionRepeatTimer = 0.0f;
            completionOwnerTab = activeTab;
            completionOwnerDocumentPath = lspDocumentPath;
            if (completionOwnerDocumentPath.empty()) {
                completionOwnerDocumentPath = tab.id;
            }
            return;
        }

        completionVisible = true;
        completionLoading = localFallback.empty();
        completionItems = localFallback;
        completionSelected = 0;
        completionScrollOffset = 0.0f;
        completionScrollbarDragging = false;
        completionRepeatKey = fst::Key::Unknown;
        completionRepeatTimer = 0.0f;
        completionOwnerTab = activeTab;
        completionOwnerDocumentPath = lspDocumentPath;
        if (completionOwnerDocumentPath.empty()) {
            completionOwnerDocumentPath = tab.id;
        }

        const int requestToken = ++completionRequestToken;
        lsp.RequestCompletion(
            lspDocumentPath,
            cursor.line,
            cursor.column,
            [&, requestToken, localFallback](const std::vector<LSPCompletionItem>& items) {
            std::lock_guard<std::mutex> lock(lspMutex);
            if (requestToken != completionRequestToken) {
                return;
            }
            pendingCompletions = items.empty() ? localFallback : items;
            pendingCompletionReady = true;
        });
    };

    auto applyCompletionItem = [&](const LSPCompletionItem& item) {
        clampActiveTab();
        if (activeTab < 0) {
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        fst::TextPosition cursor = tab.editor.cursor();
        std::string insertion = item.insertText.empty() ? item.label : item.insertText;
        std::string currentText = tab.editor.getText();
        size_t cursorOffset = offsetFromPosition(currentText, cursor);
        if (cursorOffset > currentText.size()) {
            cursorOffset = currentText.size();
        }

        size_t prefixStart = cursorOffset;
        while (prefixStart > 0) {
            const char ch = currentText[prefixStart - 1];
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch) || ch == '_') {
                --prefixStart;
                continue;
            }
            break;
        }

        const std::string typedPrefix = currentText.substr(prefixStart, cursorOffset - prefixStart);
        size_t insertionOffset = cursorOffset;
        if (!typedPrefix.empty() && insertion.rfind(typedPrefix, 0) == 0) {
            insertionOffset = prefixStart;
            currentText.erase(prefixStart, cursorOffset - prefixStart);
        }

        currentText.insert(insertionOffset, insertion);
        std::string newText = std::move(currentText);

        fst::TextPosition newCursor = positionFromOffset(newText, insertionOffset + insertion.size());

        tab.editor.setText(newText);
        tab.editor.setCursor(newCursor);
        tab.dirty = (newText != tab.savedText);

        const std::string& lspDocumentPath = ensureLspDocumentPath(tab);
        if (lspActive && !lspDocumentPath.empty()) {
            if (!tab.lspOpened) {
                lsp.DidOpen(lspDocumentPath, newText);
                tab.lspOpened = true;
            } else {
                lsp.DidChange(lspDocumentPath, newText);
            }
            tab.lspTextSnapshot = newText;
        }

        closeCompletionPopup();
    };

    const auto refreshEditorsAfterThemeChange = [&]() {
        for (auto& tab : docs) {
            applyCppSyntaxHighlighting(tab->editor, tab->path.empty() ? tab->name : tab->path, ctx.theme());
        }
    };

    const auto applyPresetThemeAndRefresh = [&](int themeId) {
        config.theme = std::clamp(themeId, 0, 2);
        applyTheme(ctx, config.theme);
        editableTheme = ctx.theme();
        selectedPersonalizationPreset = config.theme;
        refreshEditorsAfterThemeChange();
    };

    const auto applyCustomThemeAndRefresh = [&](const fst::Theme& customTheme) {
        ctx.setTheme(customTheme);
        editableTheme = ctx.theme();
        refreshEditorsAfterThemeChange();
    };

    bool pendingMenuNew = false;
    bool pendingMenuOpen = false;
    bool pendingMenuSave = false;
    bool pendingMenuCloseTab = false;
    bool pendingMenuBuild = false;
    bool pendingMenuAutocomplete = false;
    bool pendingThemeChange = false;
    int pendingDockTabFocus = -1;

    enum class DockWindowId {
        Explorer = 0,
        Editor = 1,
        Console = 2,
        LspDiagnostics = 3,
        Terminal = 4,
        Settings = 5,
        Personalization = 6,
    };

    struct ManagedDockWindow {
        DockWindowId id;
        bool* visible;
        DockWindowId fallbackAnchorId;
        fst::DockDirection fallbackDirection;
    };

    const auto dockWindowTitle = [&](DockWindowId id) -> std::string {
        switch (id) {
            case DockWindowId::Explorer:
                return fst::i18n("window.explorer");
            case DockWindowId::Editor:
                return fst::i18n("window.editor");
            case DockWindowId::Console:
                return fst::i18n("window.console");
            case DockWindowId::LspDiagnostics:
                return fst::i18n("window.lsp_diagnostics");
            case DockWindowId::Terminal:
                return fst::i18n("window.terminal");
            case DockWindowId::Settings:
                return fst::i18n("window.settings");
            case DockWindowId::Personalization:
                return fst::i18n("window.personalization");
        }
        return std::string();
    };

    const std::array<ManagedDockWindow, 7> managedDockWindows = {{
        {DockWindowId::Explorer, &showExplorerTab, DockWindowId::Editor, fst::DockDirection::Left},
        {DockWindowId::Editor, &showEditorTab, DockWindowId::Explorer, fst::DockDirection::Right},
        {DockWindowId::Console, &showConsoleTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::LspDiagnostics, &showLspDiagnosticsTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::Terminal, &showTerminalTab, DockWindowId::Editor, fst::DockDirection::Bottom},
        {DockWindowId::Settings, &showSettingsWindow, DockWindowId::Editor, fst::DockDirection::Center},
        {DockWindowId::Personalization, &showPersonalizationTab, DockWindowId::Settings, fst::DockDirection::Center},
    }};

    std::unordered_map<int, fst::DockNode::Id> lastDockNodeByWindow;

    const auto dockWindowKey = [](DockWindowId id) {
        return static_cast<int>(id);
    };

    auto requestDockTab = [&](DockWindowId windowId, bool* visibilityFlag = nullptr) {
        if (visibilityFlag) {
            *visibilityFlag = true;
        }
        pendingDockTabFocus = dockWindowKey(windowId);
    };

    const auto buildMenuBar = [&]() {
        menuBar.clear();

        std::vector<fst::MenuItem> fileItems;
        fileItems.emplace_back("new", fst::i18n("menu.file.new"), [&]() { pendingMenuNew = true; }).withShortcut("Ctrl+N");
        fileItems.emplace_back("open", fst::i18n("menu.file.open"), [&]() { pendingMenuOpen = true; }).withShortcut("Ctrl+O");
        fileItems.emplace_back("save", fst::i18n("menu.file.save"), [&]() { pendingMenuSave = true; }).withShortcut("Ctrl+S");
        fileItems.emplace_back("close", fst::i18n("menu.file.close_tab"), [&]() { pendingMenuCloseTab = true; }).withShortcut("Ctrl+W");
        fileItems.push_back(fst::MenuItem::separator());
        fileItems.emplace_back("exit", fst::i18n("menu.file.exit"), [&]() { window.close(); });
        menuBar.addMenu(fst::i18n("menu.file"), fileItems);

        std::vector<fst::MenuItem> editItems;
        editItems.emplace_back("autocomplete", fst::i18n("menu.edit.autocomplete"), [&]() { pendingMenuAutocomplete = true; }).withShortcut("Ctrl+Space");
        menuBar.addMenu(fst::i18n("menu.edit"), editItems);

        std::vector<fst::MenuItem> buildItems;
        buildItems.emplace_back("build_run", fst::i18n("menu.build.run"), [&]() { pendingMenuBuild = true; }).withShortcut("F5");
        menuBar.addMenu(fst::i18n("menu.build"), buildItems);

        std::vector<fst::MenuItem> viewItems;
        viewItems.emplace_back("view_explorer", fst::i18n("menu.view.explorer"), [&]() { requestDockTab(DockWindowId::Explorer, &showExplorerTab); });
        viewItems.emplace_back("view_editor", fst::i18n("menu.view.editor"), [&]() { requestDockTab(DockWindowId::Editor, &showEditorTab); });
        viewItems.emplace_back("view_console", fst::i18n("menu.view.console"), [&]() { requestDockTab(DockWindowId::Console, &showConsoleTab); });
        viewItems.emplace_back("view_lsp_diagnostics", fst::i18n("menu.view.lsp_diagnostics"), [&]() { requestDockTab(DockWindowId::LspDiagnostics, &showLspDiagnosticsTab); });
        viewItems.emplace_back("view_terminal", fst::i18n("menu.view.terminal"), [&]() { requestDockTab(DockWindowId::Terminal, &showTerminalTab); });
        viewItems.emplace_back("view_settings", fst::i18n("menu.view.settings"), [&]() { requestDockTab(DockWindowId::Settings, &showSettingsWindow); });
        viewItems.emplace_back("view_personalization", fst::i18n("menu.view.personalization"), [&]() { requestDockTab(DockWindowId::Personalization, &showPersonalizationTab); });
        viewItems.push_back(fst::MenuItem::separator());
        viewItems.emplace_back("theme_dark", fst::i18n("menu.view.theme_dark"), [&]() { config.theme = 0; pendingThemeChange = true; });
        viewItems.emplace_back("theme_light", fst::i18n("menu.view.theme_light"), [&]() { config.theme = 1; pendingThemeChange = true; });
        viewItems.emplace_back("theme_retro", fst::i18n("menu.view.theme_retro"), [&]() { config.theme = 2; pendingThemeChange = true; });
        menuBar.addMenu(fst::i18n("menu.view"), viewItems);
    };

    bool menuNeedsRebuild = false;
    buildMenuBar();

    const auto focusDockTab = [&](DockWindowId dockWindowId) {
        const std::string windowTitle = dockWindowTitle(dockWindowId);
        if (windowTitle.empty()) {
            return;
        }

        const fst::WidgetId widgetId = ctx.makeId(windowTitle);
        fst::DockNode* node = ctx.docking().getWindowDockNode(widgetId);
        if (!node) {
            return;
        }

        const auto it = std::find(node->dockedWindows.begin(), node->dockedWindows.end(), widgetId);
        if (it == node->dockedWindows.end()) {
            return;
        }

        node->selectedTabIndex = static_cast<int>(std::distance(node->dockedWindows.begin(), it));
    };

    const auto findManagedDockWindow = [&](DockWindowId id) -> const ManagedDockWindow* {
        const auto it = std::find_if(
            managedDockWindows.begin(),
            managedDockWindows.end(),
            [&](const ManagedDockWindow& windowInfo) {
                return id == windowInfo.id;
            });
        if (it == managedDockWindows.end()) {
            return nullptr;
        }
        return &(*it);
    };

    const auto rememberCurrentDockNodes = [&]() {
        for (const ManagedDockWindow& windowInfo : managedDockWindows) {
            const std::string windowTitle = dockWindowTitle(windowInfo.id);
            const fst::WidgetId windowId = ctx.makeId(windowTitle);
            fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
            if (node && node->hasWindow(windowId)) {
                lastDockNodeByWindow[dockWindowKey(windowInfo.id)] = node->id;
            }
        }
    };

    const auto setDockWindowVisibility = [&](DockWindowId windowKind, bool visible) {
        const ManagedDockWindow* windowInfo = findManagedDockWindow(windowKind);
        if (!windowInfo) {
            return;
        }

        if (visible == *windowInfo->visible) {
            if (visible) {
                pendingDockTabFocus = dockWindowKey(windowKind);
            }
            return;
        }

        const std::string windowTitle = dockWindowTitle(windowInfo->id);
        const fst::WidgetId windowId = ctx.makeId(windowTitle);
        fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
        if (!visible && node && node->hasWindow(windowId) && node->dockedWindows.size() <= 1) {
            return;
        }

        *windowInfo->visible = visible;
        if (visible) {
            pendingDockTabFocus = dockWindowKey(windowKind);
        }
    };

    const auto syncDockWindowVisibility = [&]() {
        rememberCurrentDockNodes();

        for (const ManagedDockWindow& windowInfo : managedDockWindows) {
            if (*windowInfo.visible) {
                continue;
            }

            const std::string windowTitle = dockWindowTitle(windowInfo.id);
            const fst::WidgetId windowId = ctx.makeId(windowTitle);
            fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
            if (node && node->hasWindow(windowId)) {
                if (node->dockedWindows.size() <= 1) {
                    *windowInfo.visible = true;
                    continue;
                }
                node->removeWindow(windowId);
            }
        }

        for (const ManagedDockWindow& windowInfo : managedDockWindows) {
            if (!*windowInfo.visible) {
                continue;
            }

            const std::string windowTitle = dockWindowTitle(windowInfo.id);
            const fst::WidgetId windowId = ctx.makeId(windowTitle);
            fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
            if (node && node->hasWindow(windowId)) {
                continue;
            }

            fst::DockNode* targetNode = nullptr;
            const auto remembered = lastDockNodeByWindow.find(dockWindowKey(windowInfo.id));
            if (remembered != lastDockNodeByWindow.end()) {
                targetNode = ctx.docking().getDockNode(remembered->second);
            }

            if (targetNode) {
                ctx.docking().dockWindow(windowId, targetNode->id, fst::DockDirection::Center);
                continue;
            }

            const std::string anchorTitle = dockWindowTitle(windowInfo.fallbackAnchorId);
            const fst::WidgetId anchorId = ctx.makeId(anchorTitle);
            if (fst::DockNode* anchorNode = ctx.docking().getWindowDockNode(anchorId)) {
                ctx.docking().dockWindow(windowId, anchorNode->id, windowInfo.fallbackDirection);
                continue;
            }

            if (fst::DockNode* rootNode = ctx.docking().getDockSpace("##MainDockSpace")) {
                ctx.docking().dockWindow(windowId, rootNode->id, fst::DockDirection::Center);
            }
        }
    };

    for (const std::string& filePath : config.openFiles) {
        openDocument(filePath);
    }
    if (docs.empty()) {
        createTab(fst::i18n("tab.new_cpp"), "", "");
    }
    if (config.activeTabIndex >= 0 && config.activeTabIndex < static_cast<int>(docs.size())) {
        activeTab = config.activeTabIndex;
    }
    clampActiveTab();

    if (config.autocompleteEnabled && !startLsp()) {
        config.autocompleteEnabled = false;
    }

    std::string previousLocale = GetLocale();

    while (window.isOpen()) {
        window.pollEvents();

        if (isCompiling && compilationTask.valid() &&
            compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            compilationOutput = compilationTask.get();
            errorList = ParseCompilerOutput(compilationOutput);
            isCompiling = false;
            statusText = fst::i18n("status.compilation_finished");
        }

        std::string terminalChunk = terminal.GetOutput();
        if (!terminalChunk.empty()) {
            std::string normalizedChunk;
            normalizedChunk.reserve(terminalChunk.size());
            for (size_t i = 0; i < terminalChunk.size(); ++i) {
                const char ch = terminalChunk[i];
                if (ch == '\r') {
                    if (i + 1 < terminalChunk.size() && terminalChunk[i + 1] == '\n') {
                        continue;
                    }
                    normalizedChunk.push_back('\n');
                    continue;
                }
                normalizedChunk.push_back(ch);
            }
            terminalHistory += normalizedChunk;
        }
        trimBuffer(terminalHistory, 200000);

        {
            std::lock_guard<std::mutex> lock(lspMutex);
            for (const auto& [path, diags] : pendingDiagnostics) {
                for (auto& tab : docs) {
                    if (ensureLspDocumentPath(*tab) == path) {
                        tab->lspDiagnostics = diags;
                    }
                }
            }
            pendingDiagnostics.clear();

            if (pendingCompletionReady) {
                completionItems = pendingCompletions;
                completionLoading = false;
                completionSelected = completionItems.empty() ? 0 : std::min(completionSelected, static_cast<int>(completionItems.size()) - 1);
                completionScrollOffset = 0.0f;
                completionScrollbarDragging = false;
                completionRepeatKey = fst::Key::Unknown;
                completionRepeatTimer = 0.0f;
                pendingCompletionReady = false;
            }
        }

        ctx.beginFrame(window);
        auto& input = ctx.input();

        if (input.modifiers().ctrl && input.scrollDelta().y != 0.0f) {
            textScale += input.scrollDelta().y * 0.1f;
            textScale = std::clamp(textScale, 0.5f, 3.0f);
            config.zoom = textScale;
        }

        bool actionNew = pendingMenuNew;
        bool actionOpen = pendingMenuOpen;
        bool actionSave = pendingMenuSave;
        bool actionCloseTab = pendingMenuCloseTab;
        bool actionBuild = pendingMenuBuild;
        bool actionAutocomplete = pendingMenuAutocomplete;
        bool themeChanged = pendingThemeChange;

        pendingMenuNew = false;
        pendingMenuOpen = false;
        pendingMenuSave = false;
        pendingMenuCloseTab = false;
        pendingMenuBuild = false;
        pendingMenuAutocomplete = false;
        pendingThemeChange = false;

        constexpr float menuBarHeight = 28.0f;
        constexpr float statusBarHeight = 24.0f;
        const float windowW = static_cast<float>(window.width());
        const float windowH = static_cast<float>(window.height());
        if (menuNeedsRebuild) {
            buildMenuBar();
            menuNeedsRebuild = false;
        }
        menuBar.render(ctx, fst::Rect(0.0f, 0.0f, windowW, menuBarHeight));

        if (input.modifiers().ctrl && input.isKeyPressed(fst::Key::N)) actionNew = true;
        if (input.modifiers().ctrl && input.isKeyPressed(fst::Key::O)) actionOpen = true;
        if (input.modifiers().ctrl && input.isKeyPressed(fst::Key::S)) actionSave = true;
        if (input.modifiers().ctrl && input.isKeyPressed(fst::Key::W)) actionCloseTab = true;
        if (input.modifiers().ctrl && input.isKeyPressed(fst::Key::Space)) actionAutocomplete = true;
        if (input.isKeyPressed(fst::Key::F5)) actionBuild = true;
        if (completionVisible && input.isKeyPressed(fst::Key::Escape)) {
            closeCompletionPopup();
        }

        if (themeChanged) {
            applyPresetThemeAndRefresh(config.theme);
        }

        if (actionNew) {
            createTab(fst::i18n("tab.new_cpp"), "", "");
            closeCompletionPopup();
        }
        if (actionOpen) {
            const std::string chosenPath = ShowOpenFileDialog(currentPath.string(), config.language);
            if (!chosenPath.empty() && openDocument(chosenPath)) {
                currentPath = fs::path(normalizePath(chosenPath)).parent_path();
            }
            closeCompletionPopup();
        }
        if (actionSave) {
            saveActiveTab();
        }
        if (actionCloseTab) {
            closeTab(activeTab);
        }
        if (actionBuild) {
            runBuild();
        }
        if (actionAutocomplete) {
            requestCompletionForActive(true);
        }

        if (!layoutInitialized) {
            fst::DockNode::Id dockRoot = fst::DockBuilder::GetDockSpaceId(ctx, "##MainDockSpace");
            fst::DockBuilder::Begin(dockRoot);
            fst::DockBuilder::ClearDockSpace(ctx, dockRoot);

            fst::DockNode::Id leftNode = fst::DockBuilder::SplitNode(ctx, dockRoot, fst::DockDirection::Left, 0.22f);
            fst::DockNode::Id rightNode = fst::DockBuilder::GetNode(ctx, dockRoot, fst::DockDirection::Right);
            fst::DockNode::Id bottomNode = fst::DockBuilder::SplitNode(ctx, rightNode, fst::DockDirection::Bottom, 0.28f);
            fst::DockNode::Id centerNode = fst::DockBuilder::GetNode(ctx, rightNode, fst::DockDirection::Top);

            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Explorer), leftNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Editor), centerNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Console), bottomNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::LspDiagnostics), bottomNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Terminal), bottomNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Settings), centerNode);
            fst::DockBuilder::DockWindow(ctx, dockWindowTitle(DockWindowId::Personalization), centerNode);

            fst::DockBuilder::Finish();
            layoutInitialized = true;
        }

        fst::Rect dockArea(0.0f, menuBarHeight, windowW, windowH - menuBarHeight - statusBarHeight);
        fst::DockSpace(ctx, "##MainDockSpace", dockArea);
        syncDockWindowVisibility();

        if (input.isMousePressed(fst::MouseButton::Right) &&
            !ctx.isOccluded(input.mousePos()) &&
            !input.isMouseConsumed() &&
            !fst::IsMouseOverAnyMenu(ctx)) {
            std::vector<fst::DockNode*> candidateNodes;
            for (const ManagedDockWindow& windowInfo : managedDockWindows) {
                const std::string windowTitle = dockWindowTitle(windowInfo.id);
                const fst::WidgetId windowId = ctx.makeId(windowTitle);
                fst::DockNode* node = ctx.docking().getWindowDockNode(windowId);
                if (!node || !node->hasWindow(windowId)) {
                    continue;
                }

                const bool alreadyAdded = std::any_of(
                    candidateNodes.begin(),
                    candidateNodes.end(),
                    [&](fst::DockNode* existingNode) {
                        return existingNode && existingNode->id == node->id;
                    });
                if (!alreadyAdded) {
                    candidateNodes.push_back(node);
                }
            }

            fst::DockNode* clickedNode = nullptr;
            for (fst::DockNode* node : candidateNodes) {
                if (!node) {
                    continue;
                }

                const fst::Rect tabBarBounds(
                    node->bounds.x(),
                    node->bounds.y(),
                    node->bounds.width(),
                    24.0f);
                if (tabBarBounds.contains(input.mousePos())) {
                    clickedNode = node;
                    break;
                }
            }

            if (clickedNode) {
                std::vector<fst::MenuItem> tabMenuItems;
                for (size_t i = 0; i < managedDockWindows.size(); ++i) {
                    const ManagedDockWindow& windowInfo = managedDockWindows[i];
                    const std::string windowTitle = dockWindowTitle(windowInfo.id);
                    const fst::WidgetId windowId = ctx.makeId(windowTitle);
                    const bool belongsToNode = clickedNode->hasWindow(windowId);
                    const auto remembered = lastDockNodeByWindow.find(dockWindowKey(windowInfo.id));
                    const bool rememberedInNode = remembered != lastDockNodeByWindow.end() && remembered->second == clickedNode->id;
                    if (!belongsToNode && !rememberedInNode) {
                        continue;
                    }

                    fst::MenuItem item = fst::MenuItem::checkbox(
                        "toggle_dock_tab_" + std::to_string(i),
                        windowTitle,
                        *windowInfo.visible,
                        [&, id = windowInfo.id, visibleFlag = windowInfo.visible]() {
                            setDockWindowVisibility(id, !*visibleFlag);
                        });

                    if (*windowInfo.visible && belongsToNode && clickedNode->dockedWindows.size() <= 1) {
                        item.disabled();
                    }
                    tabMenuItems.emplace_back(std::move(item));
                }

                if (!tabMenuItems.empty()) {
                    fst::ShowContextMenu(ctx, tabMenuItems, input.mousePos());
                    input.consumeMouse();
                }
            }
        }

        if (pendingDockTabFocus >= 0) {
            focusDockTab(static_cast<DockWindowId>(pendingDockTabFocus));
            pendingDockTabFocus = -1;
        }

        if (showExplorerTab) {
            RenderExplorerPanel(ctx, currentPath, openDocument);
        }

        int assistPreTab = -1;
        bool assistHasPreState = false;
        std::string assistPreText;
        fst::TextPosition assistPreCursor{};
        clampActiveTab();
        if (activeTab >= 0 && activeTab < static_cast<int>(docs.size())) {
            assistPreTab = activeTab;
            assistHasPreState = true;
            assistPreText = docs[activeTab]->editor.getText();
            assistPreCursor = docs[activeTab]->editor.cursor();
        }

        if (showEditorTab) {
            RenderEditorPanel(
                ctx,
                docs,
                activeTab,
                tabControl,
                textScale,
                completionVisible,
                completionOwnerTab,
                completionOwnerDocumentPath,
                lspActive,
                lsp,
                clampActiveTab,
                closeTab,
                closeCompletionPopup);
        }

        auto applyEditorAssists = [&]() {
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

            auto matchingClosing = [](char ch) -> char {
                switch (ch) {
                    case '(': return ')';
                    case '[': return ']';
                    case '{': return '}';
                    case '"': return '"';
                    case '\'': return '\'';
                    default: return '\0';
                }
            };

            auto isClosingChar = [](char ch) {
                return ch == ')' || ch == ']' || ch == '}' || ch == '"' || ch == '\'';
            };

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
        };

        if (showEditorTab) {
            applyEditorAssists();
        }

        constexpr float completionListHeight = 260.0f;
        const auto completionItemHeight = [&]() -> float {
            if (ctx.font()) {
                return ctx.font()->lineHeight() + ctx.theme().metrics.paddingSmall * 2.0f;
            }
            return 24.0f;
        };
        const auto keepCompletionSelectionVisible = [&](float itemHeight, float viewHeight) {
            if (completionItems.empty()) {
                completionScrollOffset = 0.0f;
                return;
            }

            const float totalHeight = itemHeight * static_cast<float>(completionItems.size());
            const float maxScroll = std::max(0.0f, totalHeight - viewHeight);
            completionScrollOffset = std::clamp(completionScrollOffset, 0.0f, maxScroll);

            const float selectedTop = completionSelected * itemHeight;
            const float selectedBottom = selectedTop + itemHeight;
            if (selectedTop < completionScrollOffset) {
                completionScrollOffset = selectedTop;
            } else if (selectedBottom > completionScrollOffset + viewHeight) {
                completionScrollOffset = selectedBottom - viewHeight;
            }

            completionScrollOffset = std::clamp(completionScrollOffset, 0.0f, maxScroll);
        };

        const auto shouldRepeatCompletionNav = [&](fst::Key key) -> bool {
            if (input.isKeyPressed(key)) {
                completionRepeatKey = key;
                completionRepeatTimer = 0.35f;
                return true;
            }

            if (completionRepeatKey == key && input.isKeyDown(key)) {
                completionRepeatTimer -= ctx.deltaTime();
                if (completionRepeatTimer <= 0.0f) {
                    completionRepeatTimer = 0.05f;
                    return true;
                }
            }

            if (completionRepeatKey == key && !input.isKeyDown(key)) {
                completionRepeatKey = fst::Key::Unknown;
                completionRepeatTimer = 0.0f;
            }
            return false;
        };

        if (completionVisible && !completionLoading && !completionItems.empty()) {
            completionSelected = std::clamp(completionSelected, 0, static_cast<int>(completionItems.size()) - 1);
            const float itemHeight = completionItemHeight();
            bool selectionMovedByKeyboard = false;

            if (shouldRepeatCompletionNav(fst::Key::Down)) {
                completionSelected = std::min(completionSelected + 1, static_cast<int>(completionItems.size()) - 1);
                selectionMovedByKeyboard = true;
            }
            if (shouldRepeatCompletionNav(fst::Key::Up)) {
                completionSelected = std::max(completionSelected - 1, 0);
                selectionMovedByKeyboard = true;
            }
            if (selectionMovedByKeyboard) {
                keepCompletionSelectionVisible(itemHeight, completionListHeight);
            }
            if (input.isKeyPressed(fst::Key::Enter) || input.isKeyPressed(fst::Key::KPEnter)) {
                applyCompletionItem(completionItems[completionSelected]);
            }
        } else if (!completionVisible) {
            completionRepeatKey = fst::Key::Unknown;
            completionRepeatTimer = 0.0f;
        }

        bool completionWindowDrawn = false;
        fst::Rect completionWindowBounds;
        if (completionVisible) {
            fst::DockableWindowOptions completionOptions;
            completionOptions.open = &completionVisible;
            completionOptions.allowDocking = false;
            completionOptions.allowFloating = true;
            completionOptions.draggable = false;
            completionOptions.showTitleBar = false;
            if (fst::BeginDockableWindow(ctx, fst::i18n("window.completion"), completionOptions)) {
                const fst::Rect bounds = ctx.layout().currentBounds();
                completionWindowBounds = bounds;
                completionWindowDrawn = true;
                ctx.layout().beginContainer(bounds);

                if (completionLoading) {
                    fst::Label(ctx, fst::i18n("completion.fetching"));
                } else if (completionItems.empty()) {
                    fst::Label(ctx, fst::i18n("completion.none"));
                } else {
                    completionSelected = std::clamp(completionSelected, 0, static_cast<int>(completionItems.size()) - 1);
                    const fst::Theme& theme = ctx.theme();
                    fst::Font* font = ctx.font();
                    fst::DrawList& dl = ctx.drawList();
                    const float itemHeight = completionItemHeight();

                    const float listWidth = std::max(260.0f, bounds.width() - 20.0f);
                    const fst::Rect listRect = fst::Allocate(ctx, listWidth, completionListHeight);

                    const float totalHeight = itemHeight * static_cast<float>(completionItems.size());
                    const bool needsScrollbar = totalHeight > listRect.height();
                    const float scrollbarWidth = needsScrollbar ? std::max(10.0f, theme.metrics.scrollbarWidth) : 0.0f;
                    fst::Rect contentRect(listRect.x(), listRect.y(), listRect.width() - scrollbarWidth, listRect.height());

                    const bool listHovered = listRect.contains(input.mousePos()) && !ctx.isOccluded(input.mousePos());
                    if (listHovered) {
                        completionScrollOffset -= input.scrollDelta().y * itemHeight;
                    }
                    const float maxScroll = std::max(0.0f, totalHeight - listRect.height());
                    completionScrollOffset = std::clamp(completionScrollOffset, 0.0f, maxScroll);

                    const float listRadius = std::max(2.0f, theme.metrics.borderRadiusSmall - 2.0f);
                    const fst::Color listFillColor = theme.colors.inputBackground.darker(0.06f);
                    dl.addRectFilled(listRect, listFillColor, listRadius);

                    int hoveredIndex = -1;
                    dl.pushClipRect(contentRect);
                    for (int i = 0; i < static_cast<int>(completionItems.size()); ++i) {
                        const float rowY = contentRect.y() + i * itemHeight - completionScrollOffset;
                        if (rowY + itemHeight < contentRect.y() || rowY > contentRect.bottom()) {
                            continue;
                        }

                        const fst::Rect rowRect(contentRect.x(), rowY, contentRect.width(), itemHeight);
                        const bool selected = (i == completionSelected);
                        const bool hovered = rowRect.contains(input.mousePos()) && contentRect.contains(input.mousePos()) && !ctx.isOccluded(input.mousePos());
                        if (hovered) {
                            hoveredIndex = i;
                        }

                        if (selected) {
                            dl.addRectFilled(rowRect, theme.colors.selection);
                        } else if (hovered) {
                            dl.addRectFilled(rowRect, theme.colors.selection.withAlpha(static_cast<uint8_t>(90)));
                        }

                        if (font) {
                            const LSPCompletionItem& item = completionItems[i];
                            const float textY = rowRect.y() + (itemHeight - font->lineHeight()) * 0.5f;
                            float textX = rowRect.x() + theme.metrics.paddingSmall;
                            const std::string rowText = item.detail.empty() ? item.label : (item.label + " :: " + item.detail);
                            const std::vector<fst::TextSegment> syntax = colorizeCppSnippet(rowText, theme);

                            auto toRowColor = [&](const fst::Color& base) -> fst::Color {
                                if (selected) {
                                    return fst::Color::lerp(base, theme.colors.selectionText, 0.55f);
                                }
                                return base;
                            };

                            int cursor = 0;
                            const int rowTextLen = static_cast<int>(rowText.size());
                            for (const auto& seg : syntax) {
                                const int start = std::clamp(seg.startColumn, 0, rowTextLen);
                                const int end = std::clamp(seg.endColumn, start, rowTextLen);
                                if (start > cursor) {
                                    const std::string plain = rowText.substr(static_cast<size_t>(cursor), static_cast<size_t>(start - cursor));
                                    dl.addText(font, fst::Vec2(textX, textY), plain, toRowColor(theme.colors.text));
                                    textX += font->measureText(plain).x;
                                }
                                if (end > start) {
                                    const std::string token = rowText.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
                                    dl.addText(font, fst::Vec2(textX, textY), token, toRowColor(seg.color));
                                    textX += font->measureText(token).x;
                                }
                                cursor = end;
                            }

                            if (cursor < static_cast<int>(rowText.size())) {
                                const std::string plain = rowText.substr(static_cast<size_t>(cursor));
                                dl.addText(font, fst::Vec2(textX, textY), plain, toRowColor(theme.colors.text));
                            }
                        }
                    }
                    dl.popClipRect();

                    if (needsScrollbar) {
                        const fst::Rect track(listRect.right() - scrollbarWidth, listRect.y(), scrollbarWidth, listRect.height());
                        const float thumbHeight = std::max(20.0f, (listRect.height() / totalHeight) * listRect.height());
                        float thumbY = track.y();
                        if (maxScroll > 0.001f) {
                            const float t = std::clamp(completionScrollOffset / maxScroll, 0.0f, 1.0f);
                            thumbY += t * (track.height() - thumbHeight);
                        }
                        const fst::Rect thumb(track.x() + 2.0f, thumbY, track.width() - 4.0f, thumbHeight);

                        if (input.isMousePressedRaw(fst::MouseButton::Left) && !ctx.isOccluded(input.mousePos())) {
                            if (thumb.contains(input.mousePos())) {
                                completionScrollbarDragging = true;
                                completionScrollbarDragOffset = input.mousePos().y - thumb.y();
                            } else if (track.contains(input.mousePos())) {
                                const float newThumbTop = std::clamp(
                                    input.mousePos().y - track.y() - thumbHeight * 0.5f,
                                    0.0f,
                                    track.height() - thumbHeight);
                                const float t = (track.height() - thumbHeight) > 0.0f
                                    ? (newThumbTop / (track.height() - thumbHeight))
                                    : 0.0f;
                                completionScrollOffset = t * maxScroll;
                                completionScrollbarDragging = true;
                                completionScrollbarDragOffset = thumbHeight * 0.5f;
                            }
                        }

                        if (completionScrollbarDragging) {
                            if (input.isMouseDown(fst::MouseButton::Left)) {
                                const float newThumbTop = std::clamp(
                                    input.mousePos().y - track.y() - completionScrollbarDragOffset,
                                    0.0f,
                                    track.height() - thumbHeight);
                                const float t = (track.height() - thumbHeight) > 0.0f
                                    ? (newThumbTop / (track.height() - thumbHeight))
                                    : 0.0f;
                                completionScrollOffset = t * maxScroll;
                            } else {
                                completionScrollbarDragging = false;
                            }
                        }

                        dl.addRectFilled(track, theme.colors.scrollbarTrack);
                        const float updatedThumbY = (maxScroll > 0.001f)
                            ? (track.y() + (completionScrollOffset / maxScroll) * (track.height() - thumbHeight))
                            : track.y();
                        const fst::Rect updatedThumb(track.x() + 2.0f, updatedThumbY, track.width() - 4.0f, thumbHeight);
                        const fst::Color thumbColor = (completionScrollbarDragging || track.contains(input.mousePos()))
                            ? theme.colors.scrollbarThumbHover
                            : theme.colors.scrollbarThumb;
                        dl.addRectFilled(updatedThumb, thumbColor, std::max(2.0f, (scrollbarWidth - 4.0f) * 0.5f));
                    } else {
                        completionScrollbarDragging = false;
                    }

                    if (hoveredIndex >= 0 && input.isMousePressedRaw(fst::MouseButton::Left)) {
                        completionSelected = hoveredIndex;
                        applyCompletionItem(completionItems[completionSelected]);
                    }

                    if (completionSelected >= 0 && completionSelected < static_cast<int>(completionItems.size())) {
                        const LSPCompletionItem& selectedItem = completionItems[completionSelected];
                        const bool localItem = selectedItem.detail == "local";

                        fst::LabelOptions sourceOptions;
                        sourceOptions.color = localItem ? ctx.theme().colors.textSecondary : ctx.theme().colors.primary;
                        fst::Label(ctx, localItem ? fst::i18n("completion.source.local") : fst::i18n("completion.source.lsp"), sourceOptions);
                        if (!selectedItem.detail.empty() && !localItem) {
                            fst::LabelSecondary(ctx, fst::i18n("completion.details", {selectedItem.detail}));
                        }
                    }

                    fst::LabelSecondary(ctx, fst::i18n("completion.hint"));
                }

                ctx.layout().endContainer();
                fst::EndDockableWindow(ctx);
            }
        }
        if (showConsoleTab) {
            RenderConsolePanel(ctx, docs, activeTab, errorList, compilationOutput, openDocument, clampActiveTab);
        }
        if (showLspDiagnosticsTab) {
            RenderLspDiagnosticsPanel(ctx, docs, activeTab);
        }
        if (showTerminalTab) {
            RenderTerminalPanel(ctx, terminal, terminalHistory, terminalInput);
        }

        RenderSettingsPanel(
            ctx,
            showSettingsWindow,
            config,
            textScale,
            startLsp,
            stopLsp,
            [&](int themeId) { applyPresetThemeAndRefresh(themeId); });

        const std::string currentLocale = GetLocale();
        config.language = currentLocale;
        if (currentLocale != previousLocale) {
            previousLocale = currentLocale;
            menuNeedsRebuild = true;
            layoutInitialized = false;
            lastDockNodeByWindow.clear();
            pendingDockTabFocus = -1;
        }

        RenderPersonalizationPanel(
            ctx,
            showPersonalizationTab,
            editableTheme,
            selectedPersonalizationPreset,
            [&](int themeId) { applyPresetThemeAndRefresh(themeId); },
            [&](const fst::Theme& customTheme) { applyCustomThemeAndRefresh(customTheme); });

        if (completionVisible && completionWindowDrawn &&
            input.isMousePressedRaw(fst::MouseButton::Left)) {
            if (!completionWindowBounds.contains(input.mousePos())) {
                closeCompletionPopup();
            }
        }

        fst::DrawList& dl = ctx.drawList();
        const fst::Theme& theme = ctx.theme();
        fst::Rect statusRect(0.0f, windowH - statusBarHeight, windowW, statusBarHeight);
        dl.addRectFilled(statusRect, theme.colors.panelBackground.darker(0.1f));
        dl.addLine(fst::Vec2(statusRect.x(), statusRect.y()), fst::Vec2(statusRect.right(), statusRect.y()), theme.colors.border);

        if (ctx.font()) {
            dl.addText(ctx.font(), fst::Vec2(8.0f, statusRect.y() + 4.0f), statusText, theme.colors.textSecondary);
            if (activeTab >= 0 && activeTab < static_cast<int>(docs.size())) {
                fst::TextPosition cur = docs[activeTab]->editor.cursor();
                int diagCount = static_cast<int>(docs[activeTab]->lspDiagnostics.size());
                std::string rightText = fst::i18n("statusbar.line") + " " + std::to_string(cur.line + 1) +
                                        ", " + fst::i18n("statusbar.col") + " " + std::to_string(cur.column + 1) +
                                        " | " + fst::i18n("statusbar.diag") + " " + std::to_string(diagCount) +
                                        " | " + (docs[activeTab]->path.empty() ? fst::i18n("statusbar.new_file") : docs[activeTab]->path);
                dl.addText(ctx.font(), fst::Vec2(320.0f, statusRect.y() + 4.0f), rightText, theme.colors.textSecondary);
            }
        }

        fst::RenderDockPreview(ctx);
        menuBar.renderPopups(ctx);
        fst::RenderContextMenu(ctx);
        ctx.endFrame();
        window.swapBuffers();
    }

    config.showSettingsWindow = showSettingsWindow;
    config.zoom = textScale;
    config.windowWidth = window.width();
    config.windowHeight = window.height();
    config.lastDirectory = currentPath.string();
    config.openFiles.clear();
    for (const auto& tabPtr : docs) {
        if (!tabPtr->path.empty()) {
            config.openFiles.push_back(tabPtr->path);
        }
    }
    config.activeTabIndex = activeTab;
    SaveConfig(config);

    stopLsp();
    terminal.Stop();
    return 0;
}
