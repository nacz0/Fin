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
#include <filesystem> // <--- NOWOŚĆ: Biblioteka do obsługi plików

namespace fs = std::filesystem; // Skrót, żeby nie pisać ciągle std::filesystem

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

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    editor.SetText("// Witaj w Fin!\n// Wybierz plik z panelu po lewej stronie.");

    // --- ZMIENNE STANU ---
    std::string compilationOutput = "Gotowy.";
    std::string currentFile = ""; 
    
    // Startujemy w katalogu, w którym jest program (lub src jeśli w build)
    fs::path currentPath = fs::current_path(); 

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // --- MENU GÓRNE ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Plik")) {
                if (ImGui::MenuItem("Zapisz", "Ctrl+S")) {
                    if (currentFile.empty()) {
                         auto dest = pfd::save_file("Zapisz jako...", currentPath.string()).result();
                         if (!dest.empty()) currentFile = dest;
                    }
                    if (!currentFile.empty()) {
                        SaveFile(currentFile, editor.GetText());
                        compilationOutput = "Zapisano: " + currentFile;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Buduj")) {
                if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) {
                    if (currentFile.empty()) {
                        compilationOutput = "Blad: Najpierw zapisz plik!";
                    } else {
                        SaveFile(currentFile, editor.GetText());
                        compilationOutput = "Kompilacja " + currentFile + "...\n";
                        std::string exeName = currentFile + ".exe";
                        std::string cmd = "g++ \"" + currentFile + "\" -o \"" + exeName + "\" 2>&1";
                        std::string buildResult = ExecCommand(cmd.c_str());
                        
                        if (buildResult.empty()) {
                            compilationOutput += "Sukces! Uruchamianie...\n----------------\n";
                            compilationOutput += ExecCommand(("\"" + exeName + "\"").c_str());
                        } else {
                            compilationOutput += "Blad kompilacji:\n" + buildResult;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        std::string title = "Fin - " + (currentFile.empty() ? "Bez tytulu" : currentFile);
        glfwSetWindowTitle(window, title.c_str());

        // --- OKNO 1: EKSPLORATOR PLIKÓW (Nowość!) ---
        ImGui::Begin("Eksplorator");
        
        // Przycisk "W górę" (..)
        if (ImGui::Button(".. (W gore)")) {
            if (currentPath.has_parent_path())
                currentPath = currentPath.parent_path();
        }
        ImGui::Separator();

        // Pętla po plikach w folderze (to magia std::filesystem)
        try {
            for (const auto& entry : fs::directory_iterator(currentPath)) {
                const auto& path = entry.path();
                std::string filename = path.filename().string();
                
                // Rozróżniamy foldery i pliki
                if (entry.is_directory()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255)); // Żółty dla folderów
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        currentPath = path; // Wejdź do folderu
                    }
                    ImGui::PopStyleColor();
                } else {
                    // Biały dla plików
                    if (ImGui::Selectable(("  " + filename).c_str())) {
                        currentFile = path.string();
                        editor.SetText(OpenFile(currentFile));
                        compilationOutput = "Otwarto: " + filename;
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            ImGui::TextColored(ImVec4(1,0,0,1), "Blad dostepu do folderu!");
        }
        ImGui::End();

        // --- OKNO 2: EDYTOR ---
        ImGui::Begin("Kod Zrodlowy");
        editor.Render("CodeEditor");
        ImGui::End();

        // --- OKNO 3: KONSOLA ---
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