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
// ---------------------------

#include <iostream>
#include <vector>
#include <filesystem>
#include <future>
#include <chrono>

namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 

    // Czcionka
    float baseFontSize = 18.0f; 
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", baseFontSize);
    if (!font) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", baseFontSize);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Zmienne stanu
    std::vector<EditorTab> tabs;
    int activeTab = -1;
    int nextTabToFocus = -1;
    float textScale = config.zoom;
    
    std::string compilationOutput = "Gotowy.";
    std::vector<ParsedError> errorList;
    std::future<std::string> compilationTask; 
    bool isCompiling = false;

    // Odtwarzanie sesji
    fs::path currentPath = fs::exists(config.lastDirectory) ? config.lastDirectory : fs::current_path();
    for (const auto& filePath : config.openFiles) {
        if (fs::exists(filePath)) {
            EditorTab nt;
            nt.name = fs::path(filePath).filename().string();
            nt.path = filePath;
            nt.editor.SetText(OpenFile(filePath));
            tabs.push_back(nt);
        }
    }
    if (config.activeTabIndex >= 0 && config.activeTabIndex < (int)tabs.size()) {
        activeTab = config.activeTabIndex;
        nextTabToFocus = activeTab;
    }

    // --- PĘTLA GŁÓWNA ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Zoom (Ctrl + Scroll)
        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            textScale += io.MouseWheel * 0.1f;
            if (textScale < 0.5f) textScale = 0.5f;
            if (textScale > 3.0f) textScale = 3.0f;
        }
        io.FontGlobalScale = textScale;

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
            ImGui::EndMainMenuBar();
        }

        // --- OBSŁUGA KOMPILACJI ---
        if (isCompiling && compilationTask.valid() && compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            compilationOutput = compilationTask.get();
            errorList = ParseCompilerOutput(compilationOutput);
            isCompiling = false;
        }

        // --- LOGIKA AKCJI ---
        if (actionNew) {
            tabs.emplace_back();
            tabs.back().name = "Bez tytulu";
            nextTabToFocus = tabs.size() - 1;
        }
        if (actionOpen) {
            auto sel = pfd::open_file("Otworz", currentPath.string(), {"C++", "*.cpp *.h"}).result();
            if (!sel.empty()) {
                std::string p = sel[0];
                bool found = false;
                for(int i=0; i<tabs.size(); i++) if(tabs[i].path == p) { nextTabToFocus = i; found = true; break; }
                if(!found) {
                    EditorTab nt; nt.name = fs::path(p).filename().string(); nt.path = p;
                    nt.editor.SetText(OpenFile(p)); tabs.push_back(nt); nextTabToFocus = tabs.size()-1;
                }
            }
        }
        if (actionSave && activeTab >= 0) {
            auto& t = tabs[activeTab];
            if (t.path.empty()) {
                auto d = pfd::save_file("Zapisz", currentPath.string(), {"C++", "*.cpp"}).result();
                if (!d.empty()) { t.path = d; t.name = fs::path(d).filename().string(); }
            }
            if (!t.path.empty()) { SaveFile(t.path, t.editor.GetText()); compilationOutput = "Zapisano: " + t.name; }
        }
        if (actionCloseTab && activeTab >= 0) tabs[activeTab].isOpen = false;
        
        if (actionBuild && !isCompiling && activeTab >= 0) {
            auto& t = tabs[activeTab];
            if (!t.path.empty()) {
                SaveFile(t.path, t.editor.GetText()); isCompiling = true;
                std::string p = t.path, exe = p + ".exe", cmd = "g++ \"" + p + "\" -o \"" + exe + "\" 2>&1";
                compilationTask = std::async(std::launch::async, [cmd, exe]() {
                    std::string r = ExecCommand(cmd.c_str());
                    return r.empty() ? "Sukces!\n---\n" + ExecCommand(("\""+exe+"\"").c_str()) : "Blad:\n" + r;
                });
            } else { compilationOutput = "Blad: Zapisz plik przed kompilacja!"; }
        }

        // --- INTERFEJS ---
        
        // 1. EKSPLORATOR
        ImGui::Begin("Eksplorator");
        if (ImGui::Button(".. (W gore)") || (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
             fs::path absolutePath = fs::absolute(currentPath);
             if (absolutePath.has_parent_path()) {
                 fs::path parent = absolutePath.parent_path();
                 if (parent != absolutePath) currentPath = parent;
             }
        }
        ImGui::Separator();
        try {
            for (auto& e : fs::directory_iterator(currentPath)) {
                std::string name = e.path().filename().string();
                if (e.is_directory()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
                    if (ImGui::Selectable(("[DIR] " + name).c_str())) currentPath = e.path();
                    ImGui::PopStyleColor();
                } else {
                    std::string ext = e.path().extension().string();
                    if (ext == ".exe" || ext == ".bin") ImGui::TextDisabled("  %s [BIN]", name.c_str());
                    else if (ImGui::Selectable(("  " + name).c_str())) {
                        std::string p = e.path().string(); bool open = false;
                        for(int i=0; i<tabs.size(); i++) if(tabs[i].path == p) { nextTabToFocus = i; open = true; break; }
                        if(!open) {
                            EditorTab nt; nt.name = name; nt.path = p; nt.editor.SetText(OpenFile(p));
                            tabs.push_back(nt); nextTabToFocus = tabs.size()-1;
                        }
                    }
                }
            }
        } catch(...) {}
        ImGui::End();

        // 2. EDYTOR
        ImGui::Begin("Kod Zrodlowy", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar);
        if (tabs.empty()) {
            ImGui::TextDisabled("Brak otwartych plikow.");
        } else {
            if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
                for (int i = 0; i < (int)tabs.size(); i++) {
                    bool open = true;
                    std::string label = tabs[i].name + "##" + std::to_string(i);
                    ImGuiTabItemFlags f = (nextTabToFocus == i) ? ImGuiTabItemFlags_SetSelected : 0;

                    if (ImGui::BeginTabItem(label.c_str(), &open, f)) {
                        activeTab = i;
                        if (nextTabToFocus == i) nextTabToFocus = -1;

                        // --- SKRÓTY KLAWIATUROWE DLA KARTY ---
                        if (actionSearch) {
                            tabs[i].showSearch = !tabs[i].showSearch;
                            if (tabs[i].showSearch) {
                                tabs[i].searchFocus = true;
                                UpdateSearchInfo(tabs[i].editor, tabs[i].searchBuf, tabs[i].searchMatchCount, tabs[i].searchMatchIndex);
                            }
                        }

                        // Obsługa F3 / Shift+F3 wewnątrz aktywnej karty
                        if (tabs[i].showSearch && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            if (ImGui::IsKeyPressed(ImGuiKey_F3)) {
                                if (io.KeyShift) FindPrev(tabs[i].editor, tabs[i].searchBuf);
                                else FindNext(tabs[i].editor, tabs[i].searchBuf);
                                UpdateSearchInfo(tabs[i].editor, tabs[i].searchBuf, tabs[i].searchMatchCount, tabs[i].searchMatchIndex);
                            }
                        }

                        // --- PASEK WYSZUKIWANIA (UI) ---
                        if (tabs[i].showSearch) {
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                            ImGui::BeginChild("SearchBar", ImVec2(0, 38 * textScale), true);
                            
                            ImGui::AlignTextToFramePadding();
                            ImGui::Text("Szukaj:"); ImGui::SameLine();
                            
                            if (tabs[i].searchFocus) { 
                                ImGui::SetKeyboardFocusHere(); 
                                tabs[i].searchFocus = false; 
                            }
                            
                            ImGui::PushItemWidth(250 * textScale);
                            if (ImGui::InputText("##searchField", tabs[i].searchBuf, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
                                FindNext(tabs[i].editor, tabs[i].searchBuf);
                            }
                            if (ImGui::IsItemEdited()) {
                                UpdateSearchInfo(tabs[i].editor, tabs[i].searchBuf, tabs[i].searchMatchCount, tabs[i].searchMatchIndex);
                            }
                            ImGui::PopItemWidth();
                            
                            ImGui::SameLine();
                            ImGui::Text("%d z %d", tabs[i].searchMatchIndex, tabs[i].searchMatchCount);

                            ImGui::SameLine();
                            if (ImGui::Button("Poprzedni")) {
                                FindPrev(tabs[i].editor, tabs[i].searchBuf);
                                UpdateSearchInfo(tabs[i].editor, tabs[i].searchBuf, tabs[i].searchMatchCount, tabs[i].searchMatchIndex);
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("Nastepny")) {
                                FindNext(tabs[i].editor, tabs[i].searchBuf);
                                UpdateSearchInfo(tabs[i].editor, tabs[i].searchBuf, tabs[i].searchMatchCount, tabs[i].searchMatchIndex);
                            }
                            
                            ImGui::SameLine();
                            ImGui::TextDisabled("(F3 / Shift+F3)");

                            ImGui::SameLine(ImGui::GetWindowWidth() - 30 * textScale);
                            if (ImGui::Button("X")) { tabs[i].showSearch = false; }
                            
                            ImGui::EndChild();
                            ImGui::PopStyleColor();
                        }

                        // --- RENDEROWANIE EDYTORA ---
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        
                        // Logika przed renderem (Backspace, Auto-domykanie)
                        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            HandlePreRenderLogic(tabs[i].editor);
                        }
                            
                        // Główny komponent edytora
                        tabs[i].editor.Render("Editor", ImVec2(avail.x, avail.y - 30 * textScale));

                        // Logika po renderze (Smart Enter)
                        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            HandlePostRenderLogic(tabs[i].editor);
                        }

                        // Pasek statusu na dole karty
                        ImGui::Separator();
                        auto c = tabs[i].editor.GetCursorPosition();
                        ImGui::Text("Linia %d, Kolumna %d | Ogolem: %d linii | Skala: %.1fx", 
                            c.mLine + 1, c.mColumn + 1, tabs[i].editor.GetTotalLines(), textScale);
                        
                        ImGui::EndTabItem();
                    }
                    if (!open) tabs[i].isOpen = false;
                }
                ImGui::EndTabBar();
            }

            // Usuwanie kart oznaczonych jako zamknięte
            for (int i = 0; i < (int)tabs.size(); ) {
                if (!tabs[i].isOpen) { 
                    tabs.erase(tabs.begin() + i); 
                    if (activeTab >= i && activeTab > 0) activeTab--; 
                } else {
                    i++;
                }
            }
        }
        ImGui::End();
        // 3. KONSOLA
        ImGui::Begin("Konsola Wyjscia");
        if (!g_SearchLog.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", g_SearchLog.c_str());
        if (ImGui::Button("Wyczysc Logi Szukania")) g_SearchLog = "";
        ImGui::Separator();
        }
        if (isCompiling) ImGui::TextColored(ImVec4(1, 1, 0, 1), "KOMPILACJA...");
        if (errorList.empty()) ImGui::TextWrapped("%s", compilationOutput.c_str());
        else {
            for (auto& err : errorList) {
                if (err.line == 0) ImGui::TextWrapped("%s", err.fullMessage.c_str());
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, err.isError ? ImVec4(1,0.4,0.4,1) : ImVec4(1,1,0.4,1));
                    if (ImGui::Selectable((err.filename + ":" + std::to_string(err.line) + " " + err.message).c_str())) {
                        for(int i=0; i<tabs.size(); i++) {
                            if(fs::path(tabs[i].path).filename() == fs::path(err.filename).filename()) {
                                nextTabToFocus = i; tabs[i].editor.SetCursorPosition(TextEditor::Coordinates(err.line-1, 0)); break;
                            }
                        }
                    }
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::End();

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
    for(const auto& t : tabs) if(!t.path.empty()) config.openFiles.push_back(t.path);
    config.activeTabIndex = activeTab;
    SaveConfig(config);

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();
    return 0;
}