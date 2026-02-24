cmake_minimum_required(VERSION 3.16)

project(Fin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Fastener as a local dependency.
set(FST_BUILD_EXAMPLES OFF CACHE BOOL "Disable Fastener examples" FORCE)
set(FST_BUILD_TESTS OFF CACHE BOOL "Disable Fastener tests" FORCE)
add_subdirectory(../Fastener Fastener-build)

add_executable(Fin
    src/main.cpp
    src/App/FinApp.cpp
    src/App/FinHelpers.cpp
    src/App/Panels/ExplorerPanel.cpp
    src/App/Panels/EditorPanel.cpp
    src/App/Panels/ConsolePanel.cpp
    src/App/Panels/TerminalPanel.cpp
    src/App/Panels/SettingsPanel.cpp
    src/Core/Compiler.cpp
    src/Core/ConfigManager.cpp
    src/Core/FileManager.cpp
    src/Core/LSPClient.cpp
    src/Core/Terminal.cpp
)

target_include_directories(Fin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

target_link_libraries(Fin PRIVATE fastener)
