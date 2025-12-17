#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "TextEditor.h"
#include <GLFW/glfw3.h>

// --- NOWE BIBLIOTEKI ---
#include <iostream>
#include <fstream>  // Do zapisu plików
#include <string>
#include <sstream>  // Do łączenia tekstów
#include <array>    // Do bufora kompilatora
#include <cstdio>   // Do _popen (uruchamianie procesów)
// -----------------------
// 1. Funkcja do zapisu tekstu do pliku
void SaveFile(const std::string& filename, const std::string& text) {
    std::ofstream out(filename);
    out << text;
    out.close();
}

// 2. Funkcja do odczytu pliku
std::string OpenFile(const std::string& filename) {
    std::ifstream in(filename);
    if (in) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return "";
}

// 3. Funkcja uruchamiająca komendę (np. g++) i zwracająca to, co ona wypisze
std::string ExecCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // _popen otwiera "rurę" do procesu. Na Linuxie byłoby to popen.
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        return "popen() failed!";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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

    // --- NOWE: Konfiguracja Edytora ---
    TextEditor editor;
    auto lang = TextEditor::LanguageDefinition::CPlusPlus();
    editor.SetLanguageDefinition(lang);
    
    // Wpiszmy jakiś przykładowy kod na start
    editor.SetText("int main() {\n\treturn 0;\n}"); 

    // Zmienna przechowująca wynik kompilacji (błędy lub sukces)
    std::string compilationOutput = "Gotowy do pracy...";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. DOCKSPACE (Musi być na początku)
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // 2. GÓRNE MENU (File, Build)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Plik")) {
                if (ImGui::MenuItem("Zapisz", "Ctrl+S")) {
                    SaveFile("kod.cpp", editor.GetText());
                    compilationOutput = "Zapisano plik: kod.cpp";
                }
                if (ImGui::MenuItem("Otwórz", "Ctrl+O")) {
                    std::string content = OpenFile("kod.cpp");
                    editor.SetText(content);
                    compilationOutput = "Wczytano plik: kod.cpp";
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Buduj")) {
                if (ImGui::MenuItem("Kompiluj i Uruchom", "F5")) {
                    // A. Zapisz aktualny kod
                    SaveFile("kod.cpp", editor.GetText());
                    
                    // B. Uruchom g++
                    // 2>&1 oznacza "przekieruj błędy do standardowego wyjścia", żebyśmy je widzieli
                    compilationOutput = "Kompilacja...\n";
                    std::string buildResult = ExecCommand("g++ kod.cpp -o program.exe 2>&1");
                    
                    if (buildResult.empty()) {
                        compilationOutput += "Sukces! Uruchamianie...\n";
                        // C. Jeśli brak błędów, uruchom program
                        compilationOutput += ExecCommand("program.exe");
                    } else {
                        compilationOutput += "Blad kompilacji:\n" + buildResult;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // 3. OKNO EDYTORA
        ImGui::Begin("Kod Zrodlowy");
        editor.Render("CodeEditor");
        ImGui::End();

        // 4. OKNO KONSOLI (Tu wyświetlimy wyniki)
        ImGui::Begin("Konsola Wyjscia");
        ImGui::TextWrapped("%s", compilationOutput.c_str());
        // Automatyczne przewijanie do dołu, jeśli tekst jest długi
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}