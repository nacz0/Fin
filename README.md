# Fin - Fast IDE on Fastener

Fin is a lightweight C++ IDE implemented with the `fastener` UI library.

## Stack

- UI and windowing: `fastener`
- Build system: `CMake`
- Language services: `clangd` (LSP)
- Build/run integration: `g++`

## Features

- Multi-tab text editor
- File explorer with directory navigation
- Dockable layout (explorer, editor, console, terminal, settings)
- C++ compile and run (`F5`)
- Compiler error parsing with click-to-jump
- LSP diagnostics and completion
- Theme switching (dark/light/retro)
- Session persistence (open files, active tab, zoom, window size)

## Project Layout

```text
Fin/
|- cmake/
|  `- FinSources.cmake
|- docs/
|  `- ARCHITECTURE.md
|- src/
|  |- App/
|  |  |- Panels/
|  |  |- FinApp.cpp
|  |  `- FinHelpers.cpp
|  |- Core/
|  `- main.cpp
|- thirdparty/
|  `- json.hpp
`- CMakeLists.txt
```

## Build

```bash
cd Fin
cmake -S . -B build-fastener
cmake --build build-fastener --config Release
```

Executable path (Visual Studio generator):

- `build-fastener/Release/Fin.exe`

## Requirements

- Windows 10 or newer
- CMake 3.16+
- C++17 compiler (MSVC or MinGW)
- `clangd` in PATH (for autocomplete/diagnostics)
- `g++` in PATH (for F5 compile/run flow)

## Notes

- Fin is now maintained as a Fastener-only codebase.
- App runtime code lives in `src/App` and reusable infrastructure in `src/Core`.
- CMake source lists are centralized in `cmake/FinSources.cmake` for easier maintenance.
