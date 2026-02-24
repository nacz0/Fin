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
        fst::TextPosition newCursor;
        std::string insertion = item.insertText.empty() ? item.label : item.insertText;
        std::string newText = insertAtPosition(tab.editor.getText(), cursor, insertion, newCursor);

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

        const auto shouldAutoTriggerCompletion = [&]() -> bool {
            if (!config.autocompleteEnabled || input.modifiers().ctrl || input.modifiers().alt || input.modifiers().super) {
                return false;
            }

            const std::string& typed = input.textInput();
            if (!typed.empty()) {
                for (char ch : typed) {
                    const unsigned char uch = static_cast<unsigned char>(ch);
                    if (std::isalnum(uch) || ch == '_' || ch == '.' || ch == ':') {
                        return true;
                    }
                }
            }

            return input.isKeyPressed(fst::Key::Backspace) || input.isKeyPressed(fst::Key::Delete);
        };

        if (!actionAutocomplete && shouldAutoTriggerCompletion()) {
            requestCompletionForActive(false);
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
                    if (!completionItems.empty() && completionItems.front().detail == "local") {
                        fst::Label(ctx, "Podpowiedzi lokalne");
                    }
                    int maxShow = std::min(static_cast<int>(completionItems.size()), 30);
                    for (int i = 0; i < maxShow; ++i) {
                        bool selected = (i == completionSelected);
                        std::string detail = completionItems[i].detail.empty() ? std::string() : (" :: " + completionItems[i].detail);
                        std::string label = completionItems[i].label + detail;
                        if (fst::Selectable(ctx, label, selected)) {
                            completionSelected = i;
                        }
                    }

                    fst::BeginHorizontal(ctx, 8.0f);
                    if (fst::Button(ctx, "Wstaw") && completionSelected >= 0 && completionSelected < static_cast<int>(completionItems.size())) {
                        applyCompletionItem(completionItems[completionSelected]);
                    }
                    if (fst::Button(ctx, "Zamknij")) {
                        completionVisible = false;
                    }
                    fst::EndHorizontal(ctx);
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
