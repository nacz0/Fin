cmake_minimum_required(VERSION 3.16)

project(Fin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/FinSources.cmake)

# Build Fastener as a local dependency.
set(FST_BUILD_EXAMPLES OFF CACHE BOOL "Disable Fastener examples" FORCE)
set(FST_BUILD_TESTS OFF CACHE BOOL "Disable Fastener tests" FORCE)
add_subdirectory(../Fastener Fastener-build)

add_library(fin_core STATIC ${FIN_CORE_SOURCES})
target_include_directories(fin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

add_library(fin_app STATIC ${FIN_APP_SOURCES})
target_include_directories(fin_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)
target_link_libraries(fin_app PUBLIC fin_core fastener)

add_executable(Fin src/main.cpp)
target_link_libraries(Fin PRIVATE fin_app)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES
    src/main.cpp
    ${FIN_APP_SOURCES}
    ${FIN_CORE_SOURCES}
)
