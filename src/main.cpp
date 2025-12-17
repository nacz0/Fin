#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "portable-file-dialogs.h" 
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <array>
#include <cstdio>
#include <memory>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// --- NOWOŚĆ: Struktura pojedynczej karty ---
struct EditorTab {
    std::string name;       // Nazwa wyświetlana (np. "main.cpp")
    std::string path;       // Pełna ścieżka (do zapisu)
    TextEditor editor;      // Sam edytor kodu
    bool isOpen = true;     // Czy karta ma być zamknięta?

    // Konstruktor ustawiający język C++ na start
    EditorTab() {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    }
};

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void SaveFile(const std::string& filename, const std::string& text) {
    std::ofstream out(filename);
    out << text;
    out.close();
}

std::string OpenFile(const std::string& filename) {
    std::ifstream in(filename);
    if (in) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return "";
}

std::string ExecCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) return "popen() failed!";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fin - The Fast IDE", nullptr, nullptr);
    if (window == nullptr) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 

    // --- CZCIONKI (Z poprzedniego kroku) ---
    float baseFontSize = 18.0f; 
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", baseFontSize);
    if (font == nullptr) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", baseFontSize);
    ImGui::GetStyle().ScaleAllSizes(1.0f); 

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- ZMIENNE STANU ---
    std::vector<EditorTab> tabs; // Lista otwartych plików
    int activeTab = -1;          // Indeks aktualnie wybranej karty (-1 = brak)
    
    std::string compilationOutput = "Witaj w Fin! Otworz plik, aby zaczac.";
    fs::path currentPath = fs::current_path(); 

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // --- GÓRNE MENU ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Plik")) {
                if (ImGui::MenuItem("Nowy", "Ctrl+N")) {
                    // Dodaj nową pustą kartę
                    EditorTab newTab;
                    newTab.name = "Bez tytulu";
                    newTab.path = "";
                    tabs.push_back(newTab);
                    activeTab = tabs.size() - 1; // Przełącz na nową
                }
                if (ImGui::MenuItem("Zapisz", "Ctrl+S")) {
                    if (activeTab >= 0 && activeTab < tabs.size()) {
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
                            compilationOutput = "Zapisano: " + tab.path;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Buduj")) {
                if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) {
                    if (activeTab >= 0 && activeTab < tabs.size()) {
                        auto& tab = tabs[activeTab];
                        if (tab.path.empty()) {
                            compilationOutput = "Blad: Najpierw zapisz plik!";
                        } else {
                            SaveFile(tab.path, tab.editor.GetText());
                            compilationOutput = "Kompilacja " + tab.name + "...\n";
                            std::string exeName = tab.path + ".exe";
                            std::string cmd = "g++ \"" + tab.path + "\" -o \"" + exeName + "\" 2>&1";
                            std::string buildResult = ExecCommand(cmd.c_str());
                            
                            if (buildResult.empty()) {
                                compilationOutput += "Sukces! Uruchamianie...\n----------------\n";
                                compilationOutput += ExecCommand(("\"" + exeName + "\"").c_str());
                            } else {
                                compilationOutput += "Blad kompilacji:\n" + buildResult;
                            }
                        }
                    } else {
                        compilationOutput = "Nie wybrano zadnego pliku do kompilacji.";
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Tytuł okna
        std::string title = "Fin";
        if (activeTab >= 0 && activeTab < tabs.size()) 
            title += " - " + tabs[activeTab].name;
        glfwSetWindowTitle(window, title.c_str());

        // --- EKSPLORATOR ---
        ImGui::Begin("Eksplorator");
        if (ImGui::Button("..")) {
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
                    if (ImGui::Selectable(("  " + filename).c_str())) {
                        // LOGIKA OTWIERANIA W NOWEJ KARCIE
                        std::string fullPath = path.string();
                        
                        // 1. Sprawdź czy już otwarte
                        bool alreadyOpen = false;
                        for (int i = 0; i < tabs.size(); i++) {
                            if (tabs[i].path == fullPath) {
                                activeTab = i; // Przełącz na ten plik
                                alreadyOpen = true;
                                break;
                            }
                        }

                        // 2. Jeśli nie, otwórz nową kartę
                        if (!alreadyOpen) {
                            EditorTab newTab;
                            newTab.name = filename;
                            newTab.path = fullPath;
                            newTab.editor.SetText(OpenFile(fullPath));
                            tabs.push_back(newTab);
                            activeTab = tabs.size() - 1;
                        }
                    }
                }
            }
        } catch (...) {}
        ImGui::End();

        // --- OKNO EDYTORA Z ZAKŁADKAMI ---
        ImGui::Begin("Kod Zrodlowy", nullptr, ImGuiWindowFlags_MenuBar);
        
        if (tabs.empty()) {
            ImGui::Text("Brak otwartych plikow.");
            if (ImGui::Button("Utworz nowy plik")) {
                 EditorTab newTab;
                 newTab.name = "Bez tytulu";
                 tabs.push_back(newTab);
                 activeTab = 0;
            }
        } else {
            // Rysujemy pasek zakładek
            if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
                
                for (int i = 0; i < tabs.size(); i++) {
                    bool open = true;
                    
                    // --- FIX START ---
                    // Tworzymy unikalną etykietę: "NazwaPliku##Indeks"
                    // Użytkownik widzi "NazwaPliku", a ImGui widzi różnicę dzięki "##i"
                    // Ponieważ 'i' jest unikalne dla każdej karty w pętli, ID też będzie unikalne.
                    std::string label = tabs[i].name + "##" + std::to_string(i);
                    // --- FIX END ---

                    // Używamy 'label' zamiast 'tabs[i].name'
                    if (ImGui::BeginTabItem(label.c_str(), &open)) {
                        activeTab = i;
                        tabs[i].editor.Render("CodeEditor");
                        ImGui::EndTabItem();
                    }

                    if (!open) {
                        tabs[i].isOpen = false;
                    }
                }
                ImGui::EndTabBar();
            }

            // Usuwanie zamkniętych kart (poza pętlą rysowania, żeby nie psuć pamięci)
            for (int i = 0; i < tabs.size(); ) {
                if (!tabs[i].isOpen) {
                    tabs.erase(tabs.begin() + i);
                    // Korekta aktywnej karty po usunięciu
                    if (activeTab >= i && activeTab > 0) activeTab--;
                } else {
                    i++;
                }
            }
        }
        ImGui::End();

        // --- KONSOLA ---
        ImGui::Begin("Konsola Wyjscia");
        ImGui::TextWrapped("%s", compilationOutput.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}