#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "portable-file-dialogs.h" 
#include <GLFW/glfw3.h>

// --- MODUŁY PROJEKTU FIN ---
#include "Utils.h"       // Zapis/Odczyt, Config
#include "EditorTab.h"   // Struktura karty
#include "EditorLogic.h" // Nawiasy, Enter, Backspace, FindNext
#include "Compiler.h"    // Parsowanie błędów GCC
#include "ThemeManager.h"
#include "LSPClient.h"
#include "Terminal.h"
#include <mutex>
// ---------------------------

#include <iostream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// --- MODULAR UI FUNCTIONS ---

void ShowMainMenuBar(LSPClient& lsp, AppConfig& config, std::vector<std::unique_ptr<EditorTab>>& tabs, int activeTab, fs::path& currentPath, int& nextTabToFocus, bool& actionNew, bool& actionOpen, bool& actionSave, bool& actionCloseTab, bool& actionSearch, bool& actionBuild) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Plik")) {
            if (ImGui::MenuItem("Nowy", "Ctrl+N")) actionNew = true;
            if (ImGui::MenuItem("Otworz", "Ctrl+O")) actionOpen = true;
            if (ImGui::MenuItem("Zapisz", "Ctrl+S")) actionSave = true;
            if (ImGui::MenuItem("Zamknij Karte", "Ctrl+W")) actionCloseTab = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edycja")) {
            if (ImGui::MenuItem("Szukaj", "Ctrl+F")) actionSearch = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Buduj")) {
            if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) actionBuild = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Widok")) {
            if (ImGui::BeginMenu("Motyw")) {
                if (ImGui::MenuItem("Ciemny", nullptr, config.theme == 0)) {
                    config.theme = 0;
                    ThemeManager::ApplyTheme(0, tabs);
                }
                if (ImGui::MenuItem("Jasny", nullptr, config.theme == 1)) {
                    config.theme = 1;
                    ThemeManager::ApplyTheme(1, tabs);
                }
                if (ImGui::MenuItem("Retro (Niebieski)", nullptr, config.theme == 2)) {
                    config.theme = 2;
                    ThemeManager::ApplyTheme(2, tabs);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Ustawienia", nullptr, config.showSettingsWindow)) {
                config.showSettingsWindow = !config.showSettingsWindow;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void ShowSettings(AppConfig& config, float& textScale, LSPClient& lsp, std::vector<std::unique_ptr<EditorTab>>& tabs) {
    if (!config.showSettingsWindow) return;

    if (ImGui::Begin("Ustawienia", &config.showSettingsWindow)) {
        ImGui::Text("Edytor");
        ImGui::Separator();
        
        bool prevAutocomplete = config.autocompleteEnabled;
        ImGui::Checkbox("Autouzupełnianie (LSP)", &config.autocompleteEnabled);
        
        // Dynamiczne zarządzanie LSP
        if (prevAutocomplete != config.autocompleteEnabled) {
            if (config.autocompleteEnabled) {
                // Włączenie: uruchom LSP i ponownie otwórz pliki
                if (!lsp.IsRunning()) {
                    if (lsp.Start()) {
                        lsp.Initialize(fs::current_path().string());
                        for (auto& t : tabs) {
                            if (!t->path.empty()) {
                                lsp.DidOpen(t->path, t->editor.GetText());
                            }
                        }
                    }
                }
            } else {
                // Wyłączenie: zatrzymaj LSP 
                lsp.Stop();
                // [OPTIMIZATION] Clear diagnostics from all tabs when LSP is disabled
                for (auto& t : tabs) {
                    t->lspDiagnostics.clear();
                    t->editor.SetErrorMarkers(TextEditor::ErrorMarkers());
                }
            }
        }
        
        ImGui::Checkbox("Auto-domykanie nawiasów/cudzysłowów", &config.autoClosingBrackets);
        ImGui::Checkbox("Inteligentne wcięcia (Smart Indent)", &config.smartIndentEnabled);
        
        ImGui::Separator();
        ImGui::Text("Wygląd");
        if (ImGui::SliderFloat("Zoom", &textScale, 0.5f, 3.0f, "%.1fx")) {
            config.zoom = textScale;
        }
    }
    ImGui::End();
}

struct FileEntry {
    std::string name;
    std::string path;
    std::string extension;
    bool isDirectory;
};

void ShowExplorer(LSPClient& lsp, AppConfig& config, fs::path& currentPath, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus, float textScale, ImGuiIO& io) {
    static fs::path lastPath;
    static std::vector<FileEntry> cachedEntries;
    static auto lastRefresh = std::chrono::steady_clock::now();

    bool forceRefresh = false;
    ImGui::Begin("Eksplorator");
    if (ImGui::Button(".. (W gore)") || (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
         fs::path absolutePath = fs::absolute(currentPath);
         if (absolutePath.has_parent_path()) {
             fs::path parent = absolutePath.parent_path();
             if (parent != absolutePath) {
                 currentPath = parent;
                 forceRefresh = true;
             }
         }
    }
    
    // Refresh cache if path changed or time elapsed (1s)
    auto now = std::chrono::steady_clock::now();
    if (currentPath != lastPath || forceRefresh || std::chrono::duration_cast<std::chrono::seconds>(now - lastRefresh).count() >= 1) {
        cachedEntries.clear();
        try {
            for (auto& e : fs::directory_iterator(currentPath)) {
                cachedEntries.push_back({
                    e.path().filename().string(),
                    e.path().string(),
                    e.path().extension().string(),
                    e.is_directory()
                });
            }
            // Sort: directories first, then alphabetical
            std::sort(cachedEntries.begin(), cachedEntries.end(), [](const FileEntry& a, const FileEntry& b) {
                if (a.isDirectory != b.isDirectory) return a.isDirectory;
                return a.name < b.name;
            });
        } catch(...) {}
        lastPath = currentPath;
        lastRefresh = now;
    }

    ImGui::Separator();
    
    for (const auto& entry : cachedEntries) {
        if (entry.isDirectory) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
            if (ImGui::Selectable(("[DIR] " + entry.name).c_str())) {
                currentPath = entry.path;
            }
            ImGui::PopStyleColor();
        } else {
            if (entry.extension == ".exe" || entry.extension == ".bin") {
                ImGui::TextDisabled("  %s [BIN]", entry.name.c_str());
            } else if (ImGui::Selectable(("  " + entry.name).c_str())) {
                std::string p = entry.path;
                bool open = false;
                for(int i=0; i<tabs.size(); i++) {
                    if(tabs[i]->path == p) {
                        nextTabToFocus = i;
                        open = true;
                        break;
                    }
                }
                if(!open) {
                    auto nt = std::make_unique<EditorTab>(); 
                    nt->configRef = &config;
                    nt->name = entry.name; nt->path = p; nt->editor.SetText(OpenFile(p));
                    ThemeManager::ApplyTheme(config.theme, *nt);
                    if (lsp.IsRunning()) lsp.DidOpen(p, nt->editor.GetText());
                    tabs.push_back(std::move(nt)); 
                    nextTabToFocus = (int)tabs.size() - 1;
                }
            }
        }
    }
    ImGui::End();
}

void ShowConsole(bool isCompiling, const std::string& compilationOutput, std::vector<ParsedError>& errorList, std::vector<std::unique_ptr<EditorTab>>& tabs, int& nextTabToFocus) {
    ImGui::Begin("Konsola Wyjscia");
    if (isCompiling) ImGui::TextColored(ImVec4(1, 1, 0, 1), "KOMPILACJA...");
    if (errorList.empty()) ImGui::TextWrapped("%s", compilationOutput.c_str());
    else {
        for (auto& err : errorList) {
            if (err.line == 0) ImGui::TextWrapped("%s", err.fullMessage.c_str());
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, err.isError ? ImVec4(1,0.4,0.4,1) : ImVec4(1,1,0.4,1));
                if (ImGui::Selectable((err.filename + ":" + std::to_string(err.line) + " " + err.message).c_str())) {
                    for(int i=0; i<tabs.size(); i++) {
                        if(fs::path(tabs[i]->path).filename() == fs::path(err.filename).filename()) {
                            nextTabToFocus = i;
                            if (err.line > 0) {
                                tabs[i]->editor.SetCursorPosition(TextEditor::Coordinates(err.line-1, 0));
                            }
                            break;
                        }
                    }
                }
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::End();
}

void ShowTerminal(Terminal& terminal) {
    if (!ImGui::Begin("Terminal")) {
        ImGui::End();
        return;
    }
    
    static std::string history, prompt;
    static std::string totalOutput;
    
    std::string newOutput = terminal.GetOutput();
    if (!newOutput.empty()) {
        totalOutput += newOutput;
        // Limit output size
        if (totalOutput.size() > 100000) {
            totalOutput = totalOutput.substr(totalOutput.size() - 50000);
        }
        
        // Update history and prompt ONLY when new data arrives
        size_t lastNewline = totalOutput.find_last_of('\n');
        if (lastNewline != std::string::npos) {
            history = totalOutput.substr(0, lastNewline + 1);
            prompt = totalOutput.substr(lastNewline + 1);
        } else {
            history.clear();
            prompt = totalOutput;
        }
    }

    // Terminal background
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    
    // Single scrolling region
    ImGui::BeginChild("TerminalScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (!history.empty()) {
        ImGui::TextUnformatted(history.c_str());
    }

    // Render prompt and input on the same line
    ImGui::TextUnformatted(prompt.c_str());
    ImGui::SameLine(0, 0);

    // Input field styling (Native Look - Inline)
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    static char inputBuf[512] = "";
    bool reclaim_focus = false;
    
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##TerminalInput", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        terminal.SendInput(inputBuf);
        inputBuf[0] = '\0';
        reclaim_focus = true;
    }
    
    // Aggressive Focus 
    if (reclaim_focus || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive())) {
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // Always stay at bottom
    ImGui::SetScrollHereY(1.0f);
        
    ImGui::EndChild();
    
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(1);

    ImGui::End();
}

void RenderAutocompletePopup(EditorTab& tab, float textScale, LSPClient& lsp) {
    if (!lsp.IsRunning() || !tab.acState->show || tab.acState->items.empty()) {
        tab.acState->show = false;
        return;
    }
    
    std::cout << "[UI] Rendering autocomplete popup with " << tab.acState->items.size() << " items" << std::endl;
    
    // Position the popup at the current cursor screen position
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.y += ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing);
    
    // Usuwamy NoInputs i Tooltip, aby myszka działała. Używamy NoFocusOnAppearing aby nie zabierać focusu edytorowi.
    if (ImGui::Begin("Autocomplete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        for (int i = 0; i < (int)tab.acState->items.size(); i++) {
            bool selected = (i == tab.acState->selectedIndex);
            if (ImGui::Selectable(tab.acState->items[i].label.c_str(), selected)) {
                tab.editor.InsertText(tab.acState->items[i].insertText);
                tab.acState->show = false;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::End();
}


int main(int, char**) {
    if (!glfwInit()) return 1;

    // Ładowanie configu
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
    
    // --- INICJALIZACJA LSP I TERMINALA ---
    LSPClient lsp;
    Terminal terminal;
    std::mutex diagMutex;

    // Start processes immediately
    if (config.autocompleteEnabled) {
        if (lsp.Start()) {
            lsp.Initialize(fs::current_path().string());
        }
    }
    terminal.Start();

    // --- BACKGROUND LOADING (SESJA) ---
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

    // Czcionka
    float baseFontSize = 18.0f; 
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", baseFontSize);
    if (!font) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", baseFontSize);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Zastosuj zapisany motyw globalnie
    ApplyGlobalTheme(config.theme);

    std::map<std::string, std::vector<LSPDiagnostic>> pendingDiags;

    lsp.SetDiagnosticsCallback([&](const std::string& uri, const std::vector<LSPDiagnostic>& diags) {
        std::lock_guard<std::mutex> lock(diagMutex);
        std::string path = uri;
        if (path.find("file:///") == 0) path = path.substr(8);
        std::replace(path.begin(), path.end(), '/', '\\');
        pendingDiags[path] = diags;
    });

    // Zmienne stanu
    std::vector<std::unique_ptr<EditorTab>> tabs;
    int activeTab = -1;
    int nextTabToFocus = -1;
    float textScale = config.zoom;
    
    std::string compilationOutput = "Gotowy.";
    std::vector<ParsedError> errorList;
    std::future<std::string> compilationTask; 
    bool isCompiling = false;

    fs::path currentPath = fs::exists(config.lastDirectory) ? config.lastDirectory : fs::current_path();

    // --- PĘTLA GŁÓWNA ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Obsługa zakończenia ładowania w tle
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
            config.zoom = textScale; // Sync to config
        }
        io.FontGlobalScale = textScale;

        // --- ZASTOSOWANIE OCZEKUJĄCYCH DIAGNOSTYK LSP ---
        {
            std::lock_guard<std::mutex> lock(diagMutex);
            if (!pendingDiags.empty()) {
                for (auto const& [path, diags] : pendingDiags) {
                    for (auto& t : tabs) {
                        if (fs::path(t->path).lexically_normal() == fs::path(path).lexically_normal()) {
                            t->lspDiagnostics.clear();
                            TextEditor::ErrorMarkers markers;
                            for (auto& d : diags) {
                                t->lspDiagnostics.push_back({ d.line, d.message, d.severity });
                                markers[d.line + 1] = d.message;
                            }
                            t->editor.SetErrorMarkers(markers);
                        }
                    }
                }
                pendingDiags.clear();
            }
        }

        // Bezpiecznik indeksów
        if (tabs.empty()) activeTab = -1;
        else {
            if (activeTab >= (int)tabs.size()) activeTab = (int)tabs.size() - 1;
            if (activeTab < 0) activeTab = 0;
        }

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // --- SKRÓTY I MENU ---
        bool actionNew = false, actionOpen = false, actionSave = false, actionBuild = false, actionCloseTab = false;
        bool actionSearch = false; // [NOWE]
        bool ctrl = io.KeyCtrl;
        
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) actionNew = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) actionOpen = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) actionSave = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_W)) actionCloseTab = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F)) actionSearch = true; // [NOWE]
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) actionBuild = true;

        ShowMainMenuBar(lsp, config, tabs, activeTab, currentPath, nextTabToFocus, actionNew, actionOpen, actionSave, actionCloseTab, actionSearch, actionBuild);

        // --- OBSŁUGA KOMPILACJI ---
        if (isCompiling && compilationTask.valid() && compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            compilationOutput = compilationTask.get();
            errorList = ParseCompilerOutput(compilationOutput);
            isCompiling = false;
        }

        // --- LOGIKA AKCJI ---
        if (actionNew) {
            auto nt = std::make_unique<EditorTab>();
            nt->configRef = &config;
            nt->name = "Bez tytulu";
            
            // Aplikacja aktualnego motywu do nowej karty
            ThemeManager::ApplyTheme(config.theme, *nt);

            tabs.push_back(std::move(nt));
            nextTabToFocus = tabs.size() - 1;
        }
        if (actionOpen) {
            auto sel = pfd::open_file("Otworz", currentPath.string(), {"C++", "*.cpp *.h"}).result();
            if (!sel.empty()) {
                std::string p = sel[0];
                bool found = false;
                for(int i=0; i<tabs.size(); i++) if(tabs[i]->path == p) { nextTabToFocus = i; found = true; break; }
                if(!found) {
                    auto nt = std::make_unique<EditorTab>();
                    nt->configRef = &config;
                    nt->name = fs::path(p).filename().string(); nt->path = p;
                    nt->editor.SetText(OpenFile(p)); 
                    
                    // Aplikacja aktualnego motywu
                    ThemeManager::ApplyTheme(config.theme, *nt);

                    if (lsp.IsRunning()) lsp.DidOpen(p, nt->editor.GetText());
                    tabs.push_back(std::move(nt)); nextTabToFocus = tabs.size()-1;
                }
            }
        }
        if (actionSave && activeTab >= 0) {
            auto& t = tabs[activeTab];
            if (t->path.empty()) {
                auto d = pfd::save_file("Zapisz", currentPath.string(), {"C++", "*.cpp"}).result();
                if (!d.empty()) { t->path = d; t->name = fs::path(d).filename().string(); }
            }
            if (!t->path.empty()) { SaveFile(t->path, t->editor.GetText()); compilationOutput = "Zapisano: " + t->name; }
        }
        if (actionCloseTab && activeTab >= 0) tabs[activeTab]->isOpen = false;
        
        if (actionBuild && !isCompiling && activeTab >= 0) {
            auto& t = tabs[activeTab];
            if (!t->path.empty()) {
                SaveFile(t->path, t->editor.GetText()); isCompiling = true;
                std::string p = t->path, exe = p + ".exe", cmd = "g++ \"" + p + "\" -o \"" + exe + "\" 2>&1";
                compilationTask = std::async(std::launch::async, [cmd, exe]() {
                    std::string r = ExecCommand(cmd.c_str());
                    return r.empty() ? "Sukces!\n---\n" + ExecCommand(("\""+exe+"\"").c_str()) : "Blad:\n" + r;
                });
            } else { compilationOutput = "Blad: Zapisz plik przed kompilacja!"; }
        }

        // --- INTERFEJS ---
        
        // 1. EKSPLORATOR
        ShowExplorer(lsp, config, currentPath, tabs, nextTabToFocus, textScale, io);

        // 1.5 USTAWIENIA
        ShowSettings(config, textScale, lsp, tabs);

        // 2. EDYTOR
        ImGui::Begin("Kod Zrodlowy", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar);
        if (tabs.empty()) {
            ImGui::TextDisabled("Brak otwartych plikow.");
        } else {
            if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
                for (int i = 0; i < (int)tabs.size(); i++) {
                    bool open = tabs[i]->isOpen;
                    std::string label = tabs[i]->name + "##" + std::to_string(i);
                    ImGuiTabItemFlags f = (nextTabToFocus == i) ? ImGuiTabItemFlags_SetSelected : 0;

                    if (ImGui::BeginTabItem(label.c_str(), &open, f)) {
                        activeTab = i;
                        if (nextTabToFocus == i) nextTabToFocus = -1;

                        // --- SKRÓTY KLAWIATUROWE DLA KARTY ---
                        if (actionSearch) {
                            tabs[i]->showSearch = !tabs[i]->showSearch;
                            if (tabs[i]->showSearch) {
                                tabs[i]->searchFocus = true;
                                UpdateSearchInfo(tabs[i]->editor, tabs[i]->searchBuf, tabs[i]->searchMatchCount, tabs[i]->searchMatchIndex);
                            }
                        }

                        // Obsługa F3 / Shift+F3 wewnątrz aktywnej karty
                        if (tabs[i]->showSearch && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            if (ImGui::IsKeyPressed(ImGuiKey_F3)) {
                                if (io.KeyShift) FindPrev(tabs[i]->editor, tabs[i]->searchBuf);
                                else FindNext(tabs[i]->editor, tabs[i]->searchBuf);
                                UpdateSearchInfo(tabs[i]->editor, tabs[i]->searchBuf, tabs[i]->searchMatchCount, tabs[i]->searchMatchIndex);
                            }
                        }

                        // --- PASEK WYSZUKIWANIA (UI) ---
                        if (tabs[i]->showSearch) {
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                            ImGui::BeginChild("SearchBar", ImVec2(0, 38 * textScale), true);
                            
                            ImGui::AlignTextToFramePadding();
                            ImGui::Text("Szukaj:"); ImGui::SameLine();
                            
                            if (tabs[i]->searchFocus) { 
                                ImGui::SetKeyboardFocusHere(); 
                                tabs[i]->searchFocus = false; 
                            }
                            
                            ImGui::PushItemWidth(250 * textScale);
                            if (ImGui::InputText("##searchField", tabs[i]->searchBuf, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
                                FindNext(tabs[i]->editor, tabs[i]->searchBuf);
                            }
                            if (ImGui::IsItemEdited()) {
                                UpdateSearchInfo(tabs[i]->editor, tabs[i]->searchBuf, tabs[i]->searchMatchCount, tabs[i]->searchMatchIndex);
                            }
                            ImGui::PopItemWidth();
                            
                            ImGui::SameLine();
                            ImGui::Text("%d z %d", tabs[i]->searchMatchIndex, tabs[i]->searchMatchCount);

                            ImGui::SameLine();
                            if (ImGui::Button("Poprzedni")) {
                                FindPrev(tabs[i]->editor, tabs[i]->searchBuf);
                                UpdateSearchInfo(tabs[i]->editor, tabs[i]->searchBuf, tabs[i]->searchMatchCount, tabs[i]->searchMatchIndex);
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("Nastepny")) {
                                FindNext(tabs[i]->editor, tabs[i]->searchBuf);
                                UpdateSearchInfo(tabs[i]->editor, tabs[i]->searchBuf, tabs[i]->searchMatchCount, tabs[i]->searchMatchIndex);
                            }
                            
                            ImGui::SameLine();
                            ImGui::TextDisabled("(F3 / Shift+F3)");

                            ImGui::SameLine(ImGui::GetWindowWidth() - 30 * textScale);
                            if (ImGui::Button("X")) { tabs[i]->showSearch = false; }
                            
                            ImGui::EndChild();
                            ImGui::PopStyleColor();
                        }

                        // --- RENDEROWANIE EDYTORA ---
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        
                        // Logika przed renderem (Backspace, Auto-domykanie)
                        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            HandlePreRenderLogic(tabs[i]->editor, config);
                        }
                        // [FIX] Wywołujemy poza focus block, aby logika mogła sama wykryć utratę focusu i odblokować edytor
                        HandleAutocompleteLogic(*tabs[i], lsp, config);

                            
                        // Główny komponent edytora
                        tabs[i]->editor.Render("Editor", ImVec2(avail.x, avail.y - 30 * textScale));

                        // --- AUTOUZUPEŁNIANIE UI ---
                        if (tabs[i]->acState->show) {
                            RenderAutocompletePopup(*tabs[i], textScale, lsp);
                        }

                        if (tabs[i]->editor.IsTextChanged() && lsp.IsRunning()) {
                            lsp.DidChange(tabs[i]->path, tabs[i]->editor.GetText());
                        }

                        // Logika po renderze (Smart Enter)
                        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            HandlePostRenderLogic(*tabs[i]);
                        }

                        // Pasek statusu na dole karty
                        ImGui::Separator();
                        auto c = tabs[i]->editor.GetCursorPosition();
                        ImGui::Text("Linia %d, Kolumna %d | Ogolem: %d linii | Skala: %.1fx", 
                            c.mLine + 1, c.mColumn + 1, tabs[i]->editor.GetTotalLines(), textScale);
                        
                        ImGui::EndTabItem();
                    }
                    tabs[i]->isOpen = open; // Update the tab's open state
                }
                ImGui::EndTabBar();
            }

            // Usuwanie kart oznaczonych jako zamknięte
            for (int i = 0; i < (int)tabs.size(); ) {
                if (!tabs[i]->isOpen) { 
                    tabs.erase(tabs.begin() + i); 
                    if (activeTab >= i && activeTab > 0) activeTab--; 
                } else {
                    i++;
                }
            }
        }
        ImGui::End();
        // 3. KONSOLA
        ShowConsole(isCompiling, compilationOutput, errorList, tabs, nextTabToFocus);

        // 4. TERMINAL
        ShowTerminal(terminal);

        // Renderowanie klatki
        ImGui::Render();
        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh); glClearColor(0.1f, 0.1f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Zapis stanu
    int w, h; glfwGetWindowSize(window, &w, &h);
    config.windowWidth = w; config.windowHeight = h; config.zoom = textScale; config.lastDirectory = currentPath.string();
    config.openFiles.clear();
    for(const auto& t : tabs) if(!t->path.empty()) config.openFiles.push_back(t->path);
    config.activeTabIndex = activeTab;
    SaveConfig(config);

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();
    return 0;
}