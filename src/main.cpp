#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "portable-file-dialogs.h" 
#include <GLFW/glfw3.h>

// --- CORE MODULES ---
#include "Core/AppConfig.h"
#include "Core/ConfigManager.h"
#include "Core/FileManager.h"
#include "Core/LSPClient.h"
#include "Core/Terminal.h"
#include "Core/Compiler.h"

// --- EDITOR MODULES ---
#include "Editor/EditorTab.h"
#include "Editor/EditorLogic.h"
#include "Editor/SearchLogic.h"
#include "Editor/AutocompleteLogic.h"
#include "Editor/ThemeManager.h"

// --- UI MODULES ---
#include "UI/MenuBar.h"
#include "UI/SettingsPanel.h"
#include "UI/ExplorerPanel.h"
#include "UI/ConsolePanel.h"
#include "UI/TerminalPanel.h"
#include "UI/AutocompletePopup.h"

#include <iostream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <map>
#include <mutex>
#include <future>

namespace fs = std::filesystem;

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    if (!glfwInit()) return 1;

    AppConfig config = LoadConfig();

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(config.windowWidth, config.windowHeight, "Fin - Fast IDE Now", nullptr, nullptr);
    if (!window) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // --- INITIALIZATION ---
    LSPClient lsp;
    Terminal terminal;
    std::mutex diagMutex;

    if (config.autocompleteEnabled) {
        if (lsp.Start()) {
            lsp.Initialize(fs::current_path().string());
        }
    }
    terminal.Start();

    // --- BACKGROUND LOADING ---
    bool isStartingUp = true;
    std::future<std::vector<std::unique_ptr<EditorTab>>> backgroundTabs;
    backgroundTabs = std::async(std::launch::async, [&config]() {
        std::vector<std::unique_ptr<EditorTab>> loaded;
        for (const auto& filePath : config.openFiles) {
            if (fs::exists(filePath)) {
                auto nt = std::make_unique<EditorTab>();
                nt->configRef = &config;
                nt->name = fs::path(filePath).filename().string();
                nt->path = filePath;
                nt->editor.SetText(OpenFile(filePath));
                ThemeManager::ApplyTheme(config.theme, *nt);
                loaded.push_back(std::move(nt));
            }
        }
        return loaded;
    });

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 

    float baseFontSize = 18.0f; 
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", baseFontSize);
    if (!font) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", baseFontSize);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    ApplyGlobalTheme(config.theme);

    std::map<std::string, std::vector<LSPDiagnostic>> pendingDiags;

    lsp.SetDiagnosticsCallback([&](const std::string& uri, const std::vector<LSPDiagnostic>& diags) {
        std::lock_guard<std::mutex> lock(diagMutex);
        std::string path = uri;
        if (path.find("file:///") == 0) path = path.substr(8);
        std::replace(path.begin(), path.end(), '/', '\\');
        pendingDiags[path] = diags;
    });

    std::vector<std::unique_ptr<EditorTab>> tabs;
    int activeTab = -1;
    int nextTabToFocus = -1;
    float textScale = config.zoom;
    
    std::string compilationOutput = "Gotowy.";
    std::vector<ParsedError> errorList;
    std::future<std::string> compilationTask; 
    bool isCompiling = false;

    fs::path currentPath = fs::exists(config.lastDirectory) ? config.lastDirectory : fs::current_path();

    // --- MAIN LOOP ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle background loading completion
        if (isStartingUp && backgroundTabs.valid() && backgroundTabs.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            tabs = backgroundTabs.get();
            for (auto& t : tabs) {
                if (lsp.IsRunning() && !t->path.empty()) lsp.DidOpen(t->path, t->editor.GetText());
            }
            if (config.activeTabIndex >= 0 && config.activeTabIndex < (int)tabs.size()) {
                activeTab = config.activeTabIndex;
                nextTabToFocus = activeTab;
            }
            isStartingUp = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Zoom (Ctrl + Scroll)
        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            textScale += io.MouseWheel * 0.1f;
            if (textScale < 0.5f) textScale = 0.5f;
            if (textScale > 3.0f) textScale = 3.0f;
            config.zoom = textScale;
        }
        io.FontGlobalScale = textScale;

        // Apply pending LSP diagnostics
        {
            std::lock_guard<std::mutex> lock(diagMutex);
            for (auto& [path, diags] : pendingDiags) {
                for (auto& t : tabs) {
                    if (t->path == path) {
                        t->lspDiagnostics = diags;
                        TextEditor::ErrorMarkers markers;
                        for (auto& d : diags) {
                            if (d.severity <= 2) {
                                markers[d.line + 1] = d.message;
                            }
                        }
                        t->editor.SetErrorMarkers(markers);
                    }
                }
            }
            pendingDiags.clear();
        }

        // Action flags
        bool actionNew = false, actionOpen = false, actionSave = false;
        bool actionCloseTab = false, actionSearch = false, actionBuild = false;
        
        // DockSpace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleVar(3);
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // UI Panels
        ShowMainMenuBar(lsp, config, tabs, activeTab, currentPath, nextTabToFocus, actionNew, actionOpen, actionSave, actionCloseTab, actionSearch, actionBuild);
        ShowSettings(config, textScale, lsp, tabs);
        ShowExplorer(lsp, config, currentPath, tabs, nextTabToFocus, textScale, io);
        ShowConsole(isCompiling, compilationOutput, errorList, tabs, nextTabToFocus);
        ShowTerminal(terminal);

        ImGui::End(); // DockSpace

        // Handle keyboard shortcuts
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) actionNew = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) actionOpen = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) actionSave = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W)) actionCloseTab = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) actionSearch = true;
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) actionBuild = true;

        // Action handlers
        if (actionNew) {
            auto nt = std::make_unique<EditorTab>();
            nt->configRef = &config;
            nt->name = "Nowy.cpp"; nt->path = "";
            ThemeManager::ApplyTheme(config.theme, *nt);
            tabs.push_back(std::move(nt));
            nextTabToFocus = (int)tabs.size() - 1;
        }
        if (actionOpen) {
            auto f = pfd::open_file("Otworz plik", ".", {"Wszystkie pliki", "*"});
            if (!f.result().empty()) {
                std::string p = f.result()[0];
                auto nt = std::make_unique<EditorTab>();
                nt->configRef = &config;
                nt->name = fs::path(p).filename().string(); nt->path = p;
                nt->editor.SetText(OpenFile(p));
                ThemeManager::ApplyTheme(config.theme, *nt);
                if (lsp.IsRunning()) lsp.DidOpen(p, nt->editor.GetText());
                tabs.push_back(std::move(nt));
                nextTabToFocus = (int)tabs.size() - 1;
            }
        }
        if (actionSave && activeTab >= 0 && activeTab < (int)tabs.size()) {
            auto& tab = tabs[activeTab];
            if (tab->path.empty()) {
                auto f = pfd::save_file("Zapisz jako", ".", {"Wszystkie pliki", "*"});
                if (!f.result().empty()) { tab->path = f.result(); tab->name = fs::path(tab->path).filename().string(); }
            }
            if (!tab->path.empty()) SaveFile(tab->path, tab->editor.GetText());
        }
        if (actionCloseTab && activeTab >= 0 && activeTab < (int)tabs.size()) {
            tabs.erase(tabs.begin() + activeTab);
            if (activeTab >= (int)tabs.size()) activeTab = (int)tabs.size() - 1;
        }
        
        // Build action
        if (actionBuild && activeTab >= 0 && activeTab < (int)tabs.size() && !isCompiling) {
            auto& tab = tabs[activeTab];
            if (!tab->path.empty()) {
                SaveFile(tab->path, tab->editor.GetText());
                isCompiling = true;
                compilationOutput = "Kompilacja...";
                errorList.clear();
                
                std::string filePath = tab->path;
                compilationTask = std::async(std::launch::async, [filePath]() {
                    std::string exeName = fs::path(filePath).stem().string() + ".exe";
                    std::string cmd = "g++ -g \"" + filePath + "\" -o \"" + exeName + "\" 2>&1 && \"" + exeName + "\"";
                    return ExecCommand(cmd.c_str());
                });
            }
        }
        
        // Check compilation result
        if (isCompiling && compilationTask.valid() && compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            compilationOutput = compilationTask.get();
            errorList = ParseCompilerOutput(compilationOutput);
            isCompiling = false;
        }

        // Editor tabs
        static char searchQuery[256] = "";
        static bool showSearchBar = false;
        if (actionSearch) showSearchBar = !showSearchBar;

        ImGui::Begin("Edytor");
        if (ImGui::BeginTabBar("Tabs")) {
            for (int i = 0; i < (int)tabs.size(); i++) {
                bool tabOpen = true;
                unsigned int flags = 0;
                if (nextTabToFocus == i) { flags |= ImGuiTabItemFlags_SetSelected; nextTabToFocus = -1; }
                if (ImGui::BeginTabItem((tabs[i]->name + "##" + std::to_string(i)).c_str(), &tabOpen, flags)) {
                    activeTab = i;
                    auto& tab = *tabs[i];
                    
                    if (showSearchBar) {
                        ImGui::SetNextItemWidth(200);
                        ImGui::InputText("##Szukaj", searchQuery, IM_ARRAYSIZE(searchQuery));
                        ImGui::SameLine();
                        if (ImGui::Button("Dalej") || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::IsItemFocused())) FindNext(tab.editor, searchQuery);
                        ImGui::SameLine();
                        if (ImGui::Button("Wstecz")) FindPrev(tab.editor, searchQuery);
                        ImGui::SameLine();
                        int count = 0, idx = 0;
                        UpdateSearchInfo(tab.editor, searchQuery, count, idx);
                        ImGui::Text("(%d/%d)", idx, count);
                    }
                    
                    HandlePreRenderLogic(tab.editor, config);
                    tab.editor.Render("Editor");
                    HandlePostRenderLogic(tab);
                    
                    HandleAutocompleteLogic(tab, lsp, config);
                    RenderAutocompletePopup(tab, textScale, lsp);
                    
                    ImGui::EndTabItem();
                }
                if (!tabOpen) {
                    tabs.erase(tabs.begin() + i);
                    if (activeTab >= (int)tabs.size()) activeTab = (int)tabs.size() - 1;
                    i--;
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::End(); // Edytor

        // Status bar
        ImGui::Begin("Status");
        if (activeTab >= 0 && activeTab < (int)tabs.size()) {
            auto pos = tabs[activeTab]->editor.GetCursorPosition();
            ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);
            ImGui::SameLine();
            ImGui::Text("| %s", tabs[activeTab]->path.empty() ? "(nowy plik)" : tabs[activeTab]->path.c_str());
            ImGui::SameLine();
            ImGui::Text("| Diag: %d", (int)tabs[activeTab]->lspDiagnostics.size());
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Save session
    config.openFiles.clear();
    for (auto& t : tabs) { if (!t->path.empty()) config.openFiles.push_back(t->path); }
    config.activeTabIndex = activeTab;
    glfwGetWindowSize(window, &config.windowWidth, &config.windowHeight);
    config.lastDirectory = currentPath.string();
    SaveConfig(config);

    // Cleanup
    lsp.Stop();
    terminal.Stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}