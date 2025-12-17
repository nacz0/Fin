#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "portable-file-dialogs.h" 
#include <GLFW/glfw3.h>
#include <regex>
// --- NASZE MODUŁY ---
#include "Utils.h"      // Tu są funkcje OpenFile, SaveFile, ExecCommand
#include "EditorTab.h"  // Tu jest struktura EditorTab
// --------------------
#include <sstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <future>
#include <chrono>

namespace fs = std::filesystem;

// --- FUNKCJE POMOCNICZE ---

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct ParsedError {
    std::string fullMessage; // Cała linijka tekstu (np. "main.cpp:5: error...")
    std::string filename;    // Nazwa pliku
    int line = 0;            // Numer linii
    int col = 0;             // Numer kolumny
    std::string message;     // Sama treść błędu
    bool isError = true;     // Czy to error (czerwony) czy warning (żółty)
};

std::vector<ParsedError> ParseCompilerOutput(const std::string& output) {
    std::vector<ParsedError> errors;
    std::stringstream ss(output);
    std::string line;

    // Ulepszony Regex (Raw string R"(...)"):
    // 1. ^(.*)       -> Nazwa pliku (nawet ze spacjami i dyskiem C:)
    // 2. :(\d+)      -> Dwukropek i numer linii
    // 3. :(\d+)      -> Dwukropek i numer kolumny
    // 4. :\s+(.*)    -> Dwukropek, spacje i treść błędu
    std::regex errorRegex(R"(^(.*):(\d+):(\d+):\s+(.*)$)"); 
    std::smatch matches;

    while (std::getline(ss, line)) {
        // --- POPRAWKA NA WINDOWS (\r) ---
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // --------------------------------

        ParsedError err;
        err.fullMessage = line; 

        if (std::regex_search(line, matches, errorRegex)) {
            // Regex w C++ liczy całe dopasowanie jako [0], więc grupy są od [1]
            if (matches.size() >= 5) {
                err.filename = matches[1].str();
                err.line = std::stoi(matches[2].str());
                err.col = std::stoi(matches[3].str());
                err.message = matches[4].str(); 
                
                // Wykrywanie ostrzeżeń (warning) vs błędów (error)
                // Szukamy słowa "warning" w treści wiadomości
                if (err.message.find("warning") != std::string::npos || 
                    err.message.find("note") != std::string::npos) {
                    err.isError = false;
                }
            }
        }
        errors.push_back(err);
    }
    return errors;
}

// --- MAIN ---

int main(int, char**) {
    // 1. Inicjalizacja GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fin - The Fast IDE", nullptr, nullptr);
    if (window == nullptr) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    // 2. Inicjalizacja ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 

    // Ładowanie czcionki (Consolas lub Arial)
    float baseFontSize = 18.0f; 
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", baseFontSize);
    if (font == nullptr) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", baseFontSize);
    }
    ImGui::GetStyle().ScaleAllSizes(1.0f); 
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 3. Zmienne Stanu (State Variables)
    std::vector<EditorTab> tabs;        // Lista otwartych plików
    int activeTab = -1;                 // Indeks aktualnie wyświetlanej karty
    int nextTabToFocus = -1;            // Rozkaz zmiany karty (fix na focus stealing)

    // Stan kompilacji
    std::string compilationOutput = "Witaj w Fin!\nOtworz folder lub plik, aby zaczac prace.";
    std::future<std::string> compilationTask; 
    bool isCompiling = false;

    std::vector<ParsedError> errorList; // Tu trzymamy sparsowane błędy

    // Stan eksploratora plików
    fs::path currentPath = fs::current_path(); 

    // --- PĘTLA GŁÓWNA ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // A. SAFETY CLAMP (Zabezpieczenie indeksów przed crashem)
        if (tabs.empty()) {
            activeTab = -1;
        } else {
            if (activeTab >= (int)tabs.size()) activeTab = (int)tabs.size() - 1;
            if (activeTab < 0) activeTab = 0;
        }

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // B. ACTION FLAGS (Obsługa zdarzeń)
        bool actionNew = false;
        bool actionOpen = false;
        bool actionSave = false;
        bool actionBuild = false;
        bool actionCloseTab = false;

        // Skróty klawiszowe
        bool ctrl = io.KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) actionNew = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) actionOpen = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) actionSave = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_W)) actionCloseTab = true;
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) actionBuild = true;

        // Menu Górne
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Plik")) {
                if (ImGui::MenuItem("Nowy", "Ctrl+N"))   actionNew = true;
                if (ImGui::MenuItem("Otworz", "Ctrl+O")) actionOpen = true;
                if (ImGui::MenuItem("Zapisz", "Ctrl+S")) actionSave = true;
                if (ImGui::MenuItem("Zamknij Karte", "Ctrl+W")) actionCloseTab = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Buduj")) {
                if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) actionBuild = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // C. LOGIKA ASYNCHRONICZNA (Sprawdzanie kompilacji)
        if (isCompiling) {
            // Sprawdzamy czy wątek skończył (wait_for(0s) nie blokuje programu)
            if (compilationTask.valid() && 
                compilationTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                
                compilationOutput = compilationTask.get();
                errorList = ParseCompilerOutput(compilationOutput);
                isCompiling = false;
            }
        }

        // D. WYKONYWANIE AKCJI (Logic Processor)

        // 1. Nowy Plik
        if (actionNew) {
            EditorTab newTab;
            newTab.name = "Bez tytulu";
            newTab.path = "";
            tabs.push_back(newTab);
            nextTabToFocus = tabs.size() - 1; // Rozkaz przełączenia na nową
        }

        // 2. Otwórz Plik (Dialog)
        if (actionOpen) {
            auto selection = pfd::open_file("Wybierz plik C++", currentPath.string(),
                                            { "Pliki C++", "*.cpp *.h", "Wszystkie pliki", "*" }).result();
            if (!selection.empty()) {
                std::string fullPath = selection[0];
                
                // Sprawdź duplikaty
                bool alreadyOpen = false;
                for (int i = 0; i < tabs.size(); i++) {
                    if (tabs[i].path == fullPath) {
                        nextTabToFocus = i; // Już otwarty? Przełącz na niego
                        alreadyOpen = true;
                        break;
                    }
                }
                
                if (!alreadyOpen) {
                    EditorTab newTab;
                    newTab.name = fs::path(fullPath).filename().string();
                    newTab.path = fullPath;
                    newTab.editor.SetText(OpenFile(fullPath));
                    tabs.push_back(newTab);
                    nextTabToFocus = tabs.size() - 1;
                }
            }
        }

        // 3. Zapisz Plik
        if (actionSave) {
            if (!tabs.empty() && activeTab >= 0 && activeTab < tabs.size()) {
                auto& tab = tabs[activeTab];
                if (tab.path.empty()) {
                    auto dest = pfd::save_file("Zapisz jako...", currentPath.string()).result();
                    if (!dest.empty()) {
                        tab.path = dest;
                        tab.name = fs::path(dest).filename().string();
                    }
                }
                if (!tab.path.empty()) {
                    SaveFile(tab.path, tab.editor.GetText());
                    compilationOutput = "Zapisano: " + tab.name;
                }
            }
        }

        // 4. Zamknij Kartę
        if (actionCloseTab) {
            if (!tabs.empty() && activeTab >= 0 && activeTab < tabs.size()) {
                tabs[activeTab].isOpen = false;
            }
        }

        // 5. Buduj (Kompilacja Asynchroniczna)
        if (actionBuild) {
            if (isCompiling) {
                compilationOutput = "Kompilacja w toku! Cierpliwosci...";
            }
            else if (!tabs.empty() && activeTab >= 0 && activeTab < tabs.size()) {
                auto& tab = tabs[activeTab];
                if (tab.path.empty()) {
                    compilationOutput = "Blad: Najpierw zapisz plik!";
                } else {
                    SaveFile(tab.path, tab.editor.GetText()); // Auto-save przed kompilacją
                    
                    isCompiling = true;
                    compilationOutput = "Rozpoczynanie kompilacji...";

                    // Kopiujemy dane dla wątku
                    std::string path = tab.path;
                    std::string exeName = path + ".exe";
                    std::string cmd = "g++ \"" + path + "\" -o \"" + exeName + "\" 2>&1";

                    // Uruchamiamy w tle
                    compilationTask = std::async(std::launch::async, [cmd, exeName]() {
                        std::string res = ExecCommand(cmd.c_str());
                        if (res.empty()) {
                            return std::string("Sukces! Uruchamianie...\n----------------\n") + 
                                   ExecCommand(("\"" + exeName + "\"").c_str());
                        }
                        return std::string("Blad kompilacji:\n") + res;
                    });
                }
            } else {
                compilationOutput = "Brak otwartego pliku do kompilacji.";
            }
        }

        // E. RYSOWANIE INTERFEJSU (Rendering)

        // Tytuł Okna
        std::string title = "Fin";
        if (!tabs.empty() && activeTab >= 0 && activeTab < tabs.size()) 
            title += " - " + tabs[activeTab].name;
        glfwSetWindowTitle(window, title.c_str());

        // --- OKNO 1: EKSPLORATOR PLIKÓW ---
        ImGui::Begin("Eksplorator");
        if (ImGui::Button(".. (W gore)")) {
            if (currentPath.has_parent_path()) currentPath = currentPath.parent_path();
        }
        ImGui::Separator();

        try {
            for (const auto& entry : fs::directory_iterator(currentPath)) {
                const auto& path = entry.path();
                std::string filename = path.filename().string();
                
                if (entry.is_directory()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        currentPath = path;
                    }
                    ImGui::PopStyleColor();
                } else {
                    // FILTR BINARNY (Zabezpieczenie przed crashem)
                    std::string ext = path.extension().string();
                    bool isBinary = (ext == ".exe" || ext == ".dll" || ext == ".obj" || ext == ".o" || ext == ".bin");

                    if (isBinary) {
                        ImGui::TextDisabled("  %s [BIN]", filename.c_str());
                    } else {
                        if (ImGui::Selectable(("  " + filename).c_str())) {
                            // Logika otwierania z Eksploratora
                            std::string fullPath = path.string();
                            bool alreadyOpen = false;
                            for (int i = 0; i < tabs.size(); i++) {
                                if (tabs[i].path == fullPath) {
                                    nextTabToFocus = i; // Rozkaz przełączenia
                                    alreadyOpen = true;
                                    break;
                                }
                            }
                            if (!alreadyOpen) {
                                EditorTab newTab;
                                newTab.name = filename;
                                newTab.path = fullPath;
                                newTab.editor.SetText(OpenFile(fullPath));
                                tabs.push_back(newTab);
                                nextTabToFocus = tabs.size() - 1;
                            }
                        }
                    }
                }
            }
        } catch (...) {}
        ImGui::End();

        // --- OKNO 2: EDYTOR (ZAKŁADKI) ---
        ImGui::Begin("Kod Zrodlowy", nullptr, ImGuiWindowFlags_MenuBar);
        
        if (tabs.empty()) {
            ImGui::TextDisabled("Brak otwartych plikow. Uzyj Ctrl+N lub Eksploratora.");
        } else {
            if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
                
                for (int i = 0; i < tabs.size(); i++) {
                    bool open = true;
                    std::string label = tabs[i].name + "##" + std::to_string(i);
                    
                    ImGuiTabItemFlags flags = 0;
                    
                    // --- POPRAWKA START ---
                    // Ustawiamy flagę TYLKO gdy mamy wyraźny rozkaz zmiany (np. z Ctrl+N)
                    // Nie wymuszamy "activeTab", pozwalamy ImGui obsłużyć kliknięcia myszką
                    if (nextTabToFocus == i) {
                        flags = ImGuiTabItemFlags_SetSelected;
                        activeTab = i;     
                        nextTabToFocus = -1; // Rozkaz wykonany
                    }
                    // USUNIĘTO: else if (activeTab == i) ... <- To blokowało zmianę kart "w przód"
                    // --- POPRAWKA END ---

                    if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                        // To tutaj ImGui mówi nam: "Hej, ta karta jest teraz aktywna (bo użytkownik kliknął)"
                        activeTab = i;
                        
                        tabs[i].editor.Render("CodeEditor");
                        ImGui::EndTabItem();
                    }

                    if (!open) tabs[i].isOpen = false;
                }
                ImGui::EndTabBar();
            }

            // Usuwanie zamkniętych kart
            for (int i = 0; i < tabs.size(); ) {
                if (!tabs[i].isOpen) {
                    tabs.erase(tabs.begin() + i);
                    // Po usunięciu trzeba skorygować activeTab
                    if (activeTab >= i && activeTab > 0) activeTab--;
                    if (tabs.empty()) activeTab = -1;
                } else {
                    i++;
                }
            }
        }
        ImGui::End();

        // --- OKNO 3: KONSOLA ---
        // --- OKNO 3: KONSOLA ---
    ImGui::Begin("Konsola Wyjscia");

    if (isCompiling) {
        double time = ImGui::GetTime();
        int dots = (int)(time * 3.0) % 4;
        std::string loading = "KOMPILACJA W TOKU" + std::string(dots, '.');
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", loading.c_str());
    }

    // Jeśli lista błędów jest pusta (np. program dopiero wstał), pokaż surowy tekst
    if (errorList.empty()) {
        ImGui::TextWrapped("%s", compilationOutput.c_str());
    } 
    else {
        // Rysujemy listę błędów
        for (const auto& err : errorList) {
            // Jeśli linia nie została rozpoznana jako błąd (np. "Compilation finished"), wyświetl normalnie
            if (err.line == 0) {
                ImGui::TextWrapped("%s", err.fullMessage.c_str());
                continue;
            }

            // Wybór koloru (Czerwony dla error, Żółty dla warning)
            ImVec4 color = err.isError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(1.0f, 1.0f, 0.4f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, color);

            // Robimy z tekstu przycisk (Selectable)
            // Wyświetlamy: "main.cpp:10 [error: ...]"
            std::string label = err.filename + ":" + std::to_string(err.line) + " " + err.message;

            if (ImGui::Selectable(label.c_str())) {
                // --- KLIKNIĘCIE W BŁĄD ---

                // 1. Znajdź kartę z tym plikiem
                for (int i = 0; i < tabs.size(); i++) {
                    // Porównujemy same nazwy plików (uproszczenie)
                    if (fs::path(tabs[i].path).filename().string() == fs::path(err.filename).filename().string()) {
                        // Przełącz na tę kartę
                        nextTabToFocus = i; 

                        // Przesuń kursor do linii błędu
                        // TextEditor używa linii od 0, a kompilator od 1, więc odejmujemy 1
                        tabs[i].editor.SetCursorPosition(TextEditor::Coordinates(err.line - 1, 0)); // POPRAWKA
                        break;
                    }
                }
            }
            ImGui::PopStyleColor();

            // Tooltip po najechaniu (pokaż pełną wiadomość)
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", err.fullMessage.c_str());
            }
        }
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::End();

        // Renderowanie
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Sprzątanie
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}