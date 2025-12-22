cmake_minimum_required(VERSION 3.14)

project(Fin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Wyciszamy ostrzeżenie o FetchContent_Populate (dla świętego spokoju)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

include(FetchContent)

# 1. GLFW
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

# 2. ImGui
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui GIT_TAG docking)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# 3. TextEditor
FetchContent_Declare(color_text_edit GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit GIT_TAG master)
FetchContent_GetProperties(color_text_edit)
if(NOT color_text_edit_POPULATED)
    FetchContent_Populate(color_text_edit)
endif()

# 4. JSON for LSP (Single Header)
FetchContent_Declare(json_hpp URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(json_hpp)

# 5. Portable File Dialogs (POPRAWIONE)
FetchContent_Declare(
    pfd 
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs 
    GIT_TAG main  # <--- ZMIANA: 'master' na 'main'
)
# Ręczne pobieranie (bo biblioteka nie ma własnego CMakeLists.txt)
FetchContent_GetProperties(pfd)
if(NOT pfd_POPULATED)
    FetchContent_Populate(pfd)
endif()

add_executable(Fin 
    src/main.cpp 
    src/Utils.cpp
    src/EditorLogic.cpp
    src/Compiler.cpp
    src/ThemeManager.cpp
    src/LSPClient.cpp
    src/Terminal.cpp
)

target_include_directories(Fin PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${color_text_edit_SOURCE_DIR}
    ${pfd_SOURCE_DIR} 
    ${json_hpp_SOURCE_DIR}
)

target_sources(Fin PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${color_text_edit_SOURCE_DIR}/TextEditor.cpp
)

target_link_libraries(Fin PRIVATE glfw)

if(UNIX AND NOT APPLE)
    target_link_libraries(Fin PRIVATE GL)
elseif(APPLE)
    target_link_libraries(Fin PRIVATE "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
elseif(WIN32)
    target_link_libraries(Fin PRIVATE opengl32)
endif()