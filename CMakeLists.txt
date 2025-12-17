cmake_minimum_required(VERSION 3.14)

project(Fin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# 2. Dear ImGui (Docking)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# 3. ImGuiColorTextEdit (NOWOŚĆ: Edytor tekstu)
FetchContent_Declare(
    color_text_edit
    GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit
    GIT_TAG master
)
FetchContent_MakeAvailable(color_text_edit)

add_executable(Fin src/main.cpp)

# Włączamy nagłówki (również dla edytora)
target_include_directories(Fin PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${color_text_edit_SOURCE_DIR} # <--- Tu szukamy TextEditor.h
)

# Źródła (Sources)
target_sources(Fin PRIVATE
    # Core ImGui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    
    # Backendy
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

    # Edytor Tekstu (NOWOŚĆ)
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