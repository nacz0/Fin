#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fastener/fastener.h"

#include "App/FinApp.h"
#include "App/FinCompletionLocal.h"
#include "App/FinCompletionUi.h"
#include "App/FinDockingUi.h"
#include "App/FinEditorAssists.h"
#include "App/FinHelpers.h"
#include "App/FinI18n.h"
#include "App/FinStatusBar.h"
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
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
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
    constexpr float kBaseUiFontSize = 15.0f;
    const auto loadUiFontForScale = [&](float scale) {
        const float clampedScale = std::clamp(scale, 0.5f, 3.0f);
        const float uiFontSize = kBaseUiFontSize * clampedScale;
        if (!ctx.loadFont("C:/Windows/Fonts/consola.ttf", uiFontSize)) {
            ctx.loadFont("C:/Windows/Fonts/arial.ttf", uiFontSize);
        }
    };
    loadUiFontForScale(textScale);
    float appliedTextScale = textScale;
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

    CompletionUiState completionState;
    bool& completionVisible = completionState.visible;
    bool& completionLoading = completionState.loading;
    std::vector<LSPCompletionItem>& completionItems = completionState.items;
    int& completionOwnerTab = completionState.ownerTab;
    std::string& completionOwnerDocumentPath = completionState.ownerDocumentPath;

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
        ResetCompletionInteractionState(completionState);
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
        const std::string ownerPath = lspDocumentPath.empty() ? tab.id : lspDocumentPath;
        fst::TextPosition cursor = tab.editor.cursor();
        std::vector<LSPCompletionItem> localFallback = CollectLocalCompletions(tab, cursor);

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
            ShowCompletionPopup(completionState, activeTab, std::move(localFallback), false, ownerPath);
            return;
        }
        ShowCompletionPopup(completionState, activeTab, localFallback, localFallback.empty(), ownerPath);

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

    const std::function<void(const LSPCompletionItem&)> applyCompletionFromUi = [&](const LSPCompletionItem& item) {
        applyCompletionItem(item);
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
    ManagedDockWindows managedDockWindows = CreateManagedDockWindows(
        showExplorerTab,
        showEditorTab,
        showConsoleTab,
        showLspDiagnosticsTab,
        showTerminalTab,
        showSettingsWindow,
        showPersonalizationTab);
    std::unordered_map<int, fst::DockNode::Id> lastDockNodeByWindow;

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
        viewItems.emplace_back("view_explorer", fst::i18n("menu.view.explorer"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Explorer, &showExplorerTab); });
        viewItems.emplace_back("view_editor", fst::i18n("menu.view.editor"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Editor, &showEditorTab); });
        viewItems.emplace_back("view_console", fst::i18n("menu.view.console"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Console, &showConsoleTab); });
        viewItems.emplace_back("view_lsp_diagnostics", fst::i18n("menu.view.lsp_diagnostics"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::LspDiagnostics, &showLspDiagnosticsTab); });
        viewItems.emplace_back("view_terminal", fst::i18n("menu.view.terminal"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Terminal, &showTerminalTab); });
        viewItems.emplace_back("view_settings", fst::i18n("menu.view.settings"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Settings, &showSettingsWindow); });
        viewItems.emplace_back("view_personalization", fst::i18n("menu.view.personalization"), [&]() { RequestDockTab(pendingDockTabFocus, DockWindowId::Personalization, &showPersonalizationTab); });
        viewItems.push_back(fst::MenuItem::separator());
        viewItems.emplace_back("theme_dark", fst::i18n("menu.view.theme_dark"), [&]() { config.theme = 0; pendingThemeChange = true; });
        viewItems.emplace_back("theme_light", fst::i18n("menu.view.theme_light"), [&]() { config.theme = 1; pendingThemeChange = true; });
        viewItems.emplace_back("theme_retro", fst::i18n("menu.view.theme_retro"), [&]() { config.theme = 2; pendingThemeChange = true; });
        menuBar.addMenu(fst::i18n("menu.view"), viewItems);
    };

    bool menuNeedsRebuild = false;
    buildMenuBar();

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
                completionState.selected =
                    completionItems.empty()
                        ? 0
                        : std::min(completionState.selected, static_cast<int>(completionItems.size()) - 1);
                ResetCompletionNavigationState(completionState);
                pendingCompletionReady = false;
            }
        }

        if (std::abs(textScale - appliedTextScale) > 0.001f) {
            loadUiFontForScale(textScale);
            appliedTextScale = textScale;
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

            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Explorer), leftNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Editor), centerNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Console), bottomNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::LspDiagnostics), bottomNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Terminal), bottomNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Settings), centerNode);
            fst::DockBuilder::DockWindow(ctx, DockWindowTitle(DockWindowId::Personalization), centerNode);

            fst::DockBuilder::Finish();
            layoutInitialized = true;
        }

        fst::Rect dockArea(0.0f, menuBarHeight, windowW, windowH - menuBarHeight - statusBarHeight);
        fst::DockSpace(ctx, "##MainDockSpace", dockArea);
        SyncDockWindowVisibility(ctx, managedDockWindows, lastDockNodeByWindow);

        if (input.isMousePressed(fst::MouseButton::Right) &&
            !ctx.isOccluded(input.mousePos()) &&
            !input.isMouseConsumed() &&
            !fst::IsMouseOverAnyMenu(ctx)) {
            std::vector<fst::DockNode*> candidateNodes;
            for (const ManagedDockWindow& windowInfo : managedDockWindows) {
                const std::string windowTitle = DockWindowTitle(windowInfo.id);
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
                    const std::string windowTitle = DockWindowTitle(windowInfo.id);
                    const fst::WidgetId windowId = ctx.makeId(windowTitle);
                    const bool belongsToNode = clickedNode->hasWindow(windowId);
                    const auto remembered = lastDockNodeByWindow.find(DockWindowKey(windowInfo.id));
                    const bool rememberedInNode = remembered != lastDockNodeByWindow.end() && remembered->second == clickedNode->id;
                    if (!belongsToNode && !rememberedInNode) {
                        continue;
                    }

                    fst::MenuItem item = fst::MenuItem::checkbox(
                        "toggle_dock_tab_" + std::to_string(i),
                        windowTitle,
                        *windowInfo.visible,
                        [&, id = windowInfo.id, visibleFlag = windowInfo.visible]() {
                            SetDockWindowVisibility(
                                ctx,
                                managedDockWindows,
                                lastDockNodeByWindow,
                                pendingDockTabFocus,
                                id,
                                !*visibleFlag);
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
            FocusDockTab(ctx, static_cast<DockWindowId>(pendingDockTabFocus));
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

        if (showEditorTab) {
            ApplyEditorAssists(
                config,
                input,
                docs,
                activeTab,
                assistHasPreState,
                assistPreTab,
                assistPreText,
                assistPreCursor,
                completionVisible,
                clampActiveTab,
                closeCompletionPopup);
        }
        HandleCompletionKeyboardNavigation(ctx, input, completionState, applyCompletionFromUi);
        const CompletionWindowRenderResult completionWindowResult =
            RenderCompletionPopup(ctx, input, completionState, applyCompletionFromUi);
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

        if (completionVisible && completionWindowResult.drawn &&
            input.isMousePressedRaw(fst::MouseButton::Left)) {
            if (!completionWindowResult.bounds.contains(input.mousePos())) {
                closeCompletionPopup();
            }
        }

        RenderStatusBar(ctx, windowW, windowH, statusBarHeight, statusText, docs, activeTab);

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
