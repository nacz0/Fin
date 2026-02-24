#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fastener/fastener.h"

#include "App/FinApp.h"
#include "App/FinHelpers.h"
#include "App/FinTypes.h"
#include "App/Panels/ConsolePanel.h"
#include "App/Panels/EditorPanel.h"
#include "App/Panels/ExplorerPanel.h"
#include "App/Panels/SettingsPanel.h"
#include "App/Panels/TerminalPanel.h"

#include "Core/AppConfig.h"
#include "Core/Compiler.h"
#include "Core/ConfigManager.h"
#include "Core/FileManager.h"
#include "Core/LSPClient.h"
#include "Core/Terminal.h"

#include <algorithm>
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
    bool showOpenByPathWindow = false;
    bool showSaveAsWindow = false;

    std::string openPathInput;
    std::string saveAsPathInput;
    std::string terminalInput;
    std::string terminalHistory;
    std::string statusText = "Gotowy";
    std::string compilationOutput = "Gotowy.";

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

    auto ensureLspDocumentPath = [&](DocumentTab& tab) -> const std::string& {
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
            statusText = "Nie mozna uruchomic clangd (LSP).";
            return false;
        }

        lsp.SetDiagnosticsCallback([&](const std::string& uri, const std::vector<LSPDiagnostic>& diags) {
            const std::string path = uriToPath(uri);
            std::lock_guard<std::mutex> lock(lspMutex);
            pendingDiagnostics[path] = diags;
        });

        lsp.Initialize(fs::current_path().string());
        lspActive = true;

        for (auto& tab : docs) {
            sendDidOpenIfPossible(*tab);
        }

        statusText = "LSP uruchomiony.";
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
            if (!firstPath.empty() && firstPath.back() == '\r') {
                firstPath.pop_back();
            }
        }
        return firstPath;
    };

    auto openDocument = [&](const std::string& path) -> bool {
        if (path.empty()) {
            statusText = "Podaj sciezke pliku.";
            return false;
        }

        const std::string normalizedPath = normalizePath(path);
        std::error_code ec;
        if (!fs::exists(normalizedPath, ec) || ec) {
            statusText = "Plik nie istnieje: " + normalizedPath;
            return false;
        }

        if (!fs::is_regular_file(normalizedPath, ec) || ec) {
            statusText = "To nie jest plik: " + normalizedPath;
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
        statusText = "Otworzono: " + normalizedPath;
        if (completionVisible) {
            closeCompletionPopup();
        }
        return true;
    };

    auto saveActiveTab = [&]() {
        clampActiveTab();
        if (activeTab < 0) {
            statusText = "Brak aktywnej karty.";
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        if (tab.path.empty()) {
            saveAsPathInput = tab.name;
            showSaveAsWindow = true;
            return;
        }

        std::string text = tab.editor.getText();
        SaveFile(tab.path, text);
        tab.savedText = text;
        tab.dirty = false;
        statusText = "Zapisano: " + tab.path;
    };

    auto runBuild = [&]() {
        clampActiveTab();
        if (isCompiling || activeTab < 0) {
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        if (tab.path.empty()) {
            showSaveAsWindow = true;
            statusText = "Najpierw zapisz plik.";
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
            statusText = "Brak kompilatora clang++ i g++ w PATH.";
            return;
        }

        const std::string compilerLabel = fs::path(compilerPath).filename().string();

        isCompiling = true;
        compilationOutput = "Kompilacja (" + compilerLabel + ")...";
        errorList.clear();
        statusText = fallbackUsed
            ? ("Kompilacja fallback przez " + compilerLabel + ".")
            : ("Kompilacja przez " + compilerLabel + ".");

        const std::string sourcePath = tab.path;
        compilationTask = std::async(std::launch::async, [sourcePath, compilerPath]() {
            fs::path src(sourcePath);
            std::string exePath = (src.parent_path() / (src.stem().string() + ".exe")).string();
            std::string cmd = "\"" + compilerPath + "\" -std=c++20 -g \"" + sourcePath +
                              "\" -o \"" + exePath + "\" 2>&1 && \"" + exePath + "\"";
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
                statusText = "Brak aktywnej karty.";
            }
            return;
        }

        DocumentTab& tab = *docs[activeTab];
        const std::string& lspDocumentPath = ensureLspDocumentPath(tab);
        fst::TextPosition cursor = tab.editor.cursor();
        std::vector<LSPCompletionItem> localFallback = collectLocalCompletions(tab, cursor);

        bool canUseLsp = false;
        if (config.autocompleteEnabled) {
            if (!lspActive && !startLsp()) {
                canUseLsp = false;
            } else {
                canUseLsp = true;
            }
        }

        if (!canUseLsp || lspDocumentPath.empty()) {
            if (localFallback.empty()) {
                if (manualRequest) {
                    statusText = "Brak podpowiedzi.";
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

        auto positionFromOffset = [](const std::string& text, size_t offset) {
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
        };
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

    bool pendingMenuNew = false;
    bool pendingMenuOpen = false;
    bool pendingMenuSave = false;
    bool pendingMenuCloseTab = false;
    bool pendingMenuBuild = false;
    bool pendingMenuAutocomplete = false;
    bool pendingThemeChange = false;

    menuBar.clear();

    std::vector<fst::MenuItem> fileItems;
    fileItems.emplace_back("new", "Nowy", [&]() { pendingMenuNew = true; }).withShortcut("Ctrl+N");
    fileItems.emplace_back("open", "Otworz...", [&]() { pendingMenuOpen = true; }).withShortcut("Ctrl+O");
    fileItems.emplace_back("save", "Zapisz", [&]() { pendingMenuSave = true; }).withShortcut("Ctrl+S");
    fileItems.emplace_back("close", "Zamknij karte", [&]() { pendingMenuCloseTab = true; }).withShortcut("Ctrl+W");
    fileItems.push_back(fst::MenuItem::separator());
    fileItems.emplace_back("exit", "Zakoncz", [&]() { window.close(); });
    menuBar.addMenu("Plik", fileItems);

    std::vector<fst::MenuItem> editItems;
    editItems.emplace_back("autocomplete", "Autouzupelnianie", [&]() { pendingMenuAutocomplete = true; }).withShortcut("Ctrl+Space");
    menuBar.addMenu("Edycja", editItems);

    std::vector<fst::MenuItem> buildItems;
    buildItems.emplace_back("build_run", "Kompiluj i uruchom", [&]() { pendingMenuBuild = true; }).withShortcut("F5");
    menuBar.addMenu("Buduj", buildItems);

    std::vector<fst::MenuItem> viewItems;
    viewItems.push_back(fst::MenuItem::checkbox("settings", "Ustawienia", &showSettingsWindow));
    viewItems.push_back(fst::MenuItem::separator());
    viewItems.emplace_back("theme_dark", "Motyw: Ciemny", [&]() { config.theme = 0; pendingThemeChange = true; });
    viewItems.emplace_back("theme_light", "Motyw: Jasny", [&]() { config.theme = 1; pendingThemeChange = true; });
    viewItems.emplace_back("theme_retro", "Motyw: Retro", [&]() { config.theme = 2; pendingThemeChange = true; });
    menuBar.addMenu("Widok", viewItems);

    for (const std::string& filePath : config.openFiles) {
        openDocument(filePath);
    }
    if (docs.empty()) {
        createTab("Nowy.cpp", "", "");
    }
    if (config.activeTabIndex >= 0 && config.activeTabIndex < static_cast<int>(docs.size())) {
        activeTab = config.activeTabIndex;
    }
    clampActiveTab();

    if (config.autocompleteEnabled && !startLsp()) {
        config.autocompleteEnabled = false;
    }

    while (window.isOpen()) {
        window.pollEvents();

        if (isCompiling && compilationTask.valid() &&
            compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            compilationOutput = compilationTask.get();
            errorList = ParseCompilerOutput(compilationOutput);
            isCompiling = false;
            statusText = "Kompilacja zakonczona.";
        }

        terminalHistory += terminal.GetOutput();
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
            applyTheme(ctx, config.theme);
            for (auto& tab : docs) {
                applyCppSyntaxHighlighting(tab->editor, tab->path.empty() ? tab->name : tab->path, ctx.theme());
            }
        }

        if (actionNew) {
            createTab("Nowy.cpp", "", "");
            closeCompletionPopup();
        }
        if (actionOpen) {
            showOpenByPathWindow = true;
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

            fst::DockBuilder::DockWindow(ctx, "Eksplorator", leftNode);
            fst::DockBuilder::DockWindow(ctx, "Edytor", centerNode);
            fst::DockBuilder::DockWindow(ctx, "Konsola", bottomNode);
            fst::DockBuilder::DockWindow(ctx, "Terminal", bottomNode);
            fst::DockBuilder::DockWindow(ctx, "Ustawienia", centerNode);
            fst::DockBuilder::DockWindow(ctx, "Szybkie otwarcie", centerNode);
            fst::DockBuilder::DockWindow(ctx, "Zapisz jako", centerNode);

            fst::DockBuilder::Finish();
            layoutInitialized = true;
        }

        fst::Rect dockArea(0.0f, menuBarHeight, windowW, windowH - menuBarHeight - statusBarHeight);
        fst::DockSpace(ctx, "##MainDockSpace", dockArea);

        RenderExplorerPanel(ctx, currentPath, openDocument);

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
            if (fst::BeginDockableWindow(ctx, "Autouzupelnianie", completionOptions)) {
                const fst::Rect bounds = ctx.layout().currentBounds();
                completionWindowBounds = bounds;
                completionWindowDrawn = true;
                ctx.layout().beginContainer(bounds);

                if (completionLoading) {
                    fst::Label(ctx, "Pobieranie podpowiedzi...");
                } else if (completionItems.empty()) {
                    fst::Label(ctx, "Brak podpowiedzi.");
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

                    dl.addRectFilled(listRect, theme.colors.inputBackground, theme.metrics.borderRadiusSmall);
                    dl.addRect(listRect, theme.colors.inputBorder, theme.metrics.borderRadiusSmall);

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
                        fst::Label(ctx, localItem ? "Zrodlo: lokalne" : "Zrodlo: LSP", sourceOptions);
                        if (!selectedItem.detail.empty() && !localItem) {
                            fst::LabelSecondary(ctx, "Szczegoly: " + selectedItem.detail);
                        }
                    }

                    fst::LabelSecondary(ctx, "Strzalki = wybor, Enter = wstaw, Esc = zamknij");
                }

                ctx.layout().endContainer();
                fst::EndDockableWindow(ctx);
            }
        }
        RenderConsolePanel(ctx, docs, activeTab, errorList, compilationOutput, openDocument, clampActiveTab);

        RenderTerminalPanel(ctx, terminal, terminalHistory, terminalInput);

        RenderSettingsPanel(
            ctx,
            showSettingsWindow,
            config,
            textScale,
            startLsp,
            stopLsp,
            [&](int themeId) { applyTheme(ctx, themeId); });

        if (completionVisible && completionWindowDrawn &&
            input.isMousePressedRaw(fst::MouseButton::Left)) {
            if (!completionWindowBounds.contains(input.mousePos())) {
                closeCompletionPopup();
            }
        }

        if (showOpenByPathWindow) {
            fst::DockableWindowOptions openOptions;
            openOptions.open = &showOpenByPathWindow;
            if (fst::BeginDockableWindow(ctx, "Szybkie otwarcie", openOptions)) {
                const fst::Rect bounds = ctx.layout().currentBounds();
                ctx.layout().beginContainer(bounds);

                fst::TextInputOptions pathOptions;
                pathOptions.style = fst::Style().withWidth(std::max(160.0f, bounds.width() - 20.0f));
                (void)fst::TextInput(ctx, "open_path", openPathInput, pathOptions);

                fst::BeginHorizontal(ctx, 8.0f);
                if (fst::Button(ctx, "Otworz") && openDocument(openPathInput)) {
                    showOpenByPathWindow = false;
                }
                if (fst::Button(ctx, "Anuluj")) {
                    showOpenByPathWindow = false;
                }
                fst::EndHorizontal(ctx);

                ctx.layout().endContainer();
                fst::EndDockableWindow(ctx);
            }
        }

        if (showSaveAsWindow) {
            fst::DockableWindowOptions saveOptions;
            saveOptions.open = &showSaveAsWindow;
            if (fst::BeginDockableWindow(ctx, "Zapisz jako", saveOptions)) {
                const fst::Rect bounds = ctx.layout().currentBounds();
                ctx.layout().beginContainer(bounds);

                fst::TextInputOptions pathOptions;
                pathOptions.style = fst::Style().withWidth(std::max(160.0f, bounds.width() - 20.0f));
                (void)fst::TextInput(ctx, "save_as_path", saveAsPathInput, pathOptions);

                fst::BeginHorizontal(ctx, 8.0f);
                if (fst::Button(ctx, "Zapisz")) {
                    clampActiveTab();
                    if (activeTab >= 0 && !saveAsPathInput.empty()) {
                        const std::string previousLspPath = ensureLspDocumentPath(*docs[activeTab]);
                        docs[activeTab]->path = normalizePath(saveAsPathInput);
                        docs[activeTab]->lspDocumentPath = docs[activeTab]->path;
                        docs[activeTab]->name = fs::path(docs[activeTab]->path).filename().string();
                        applyCppSyntaxHighlighting(docs[activeTab]->editor, docs[activeTab]->path, ctx.theme());
                        std::string text = docs[activeTab]->editor.getText();
                        SaveFile(docs[activeTab]->path, text);
                        docs[activeTab]->savedText = text;
                        docs[activeTab]->dirty = false;

                        if (lspActive) {
                            if (!docs[activeTab]->lspOpened || previousLspPath != docs[activeTab]->lspDocumentPath) {
                                lsp.DidOpen(docs[activeTab]->lspDocumentPath, text);
                                docs[activeTab]->lspOpened = true;
                            } else {
                                lsp.DidChange(docs[activeTab]->lspDocumentPath, text);
                            }
                            docs[activeTab]->lspTextSnapshot = text;
                        }
                        showSaveAsWindow = false;
                    }
                }
                if (fst::Button(ctx, "Anuluj")) {
                    showSaveAsWindow = false;
                }
                fst::EndHorizontal(ctx);

                ctx.layout().endContainer();
                fst::EndDockableWindow(ctx);
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
                std::string rightText = "Ln " + std::to_string(cur.line + 1) +
                                        ", Col " + std::to_string(cur.column + 1) +
                                        " | Diag " + std::to_string(diagCount) +
                                        " | " + (docs[activeTab]->path.empty() ? "(nowy plik)" : docs[activeTab]->path);
                dl.addText(ctx.font(), fst::Vec2(320.0f, statusRect.y() + 4.0f), rightText, theme.colors.textSecondary);
            }
        }

        menuBar.renderPopups(ctx);
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
