# Fin - Fast IDE on Fastener

Fin is a lightweight C++ IDE implemented with the `fastener` UI library.

## Stack

- UI and windowing: `fastener`
- Build system: `CMake`
- Language services: `clangd` (LSP)
- Build/run integration: `clang++` / `g++` (configurable)

## Features

- Multi-tab text editor
- File explorer with directory navigation
- Dockable layout (explorer, editor, console, terminal, LSP diagnostics, settings, personalization)
- C++ compile and run (`F5`)
- Compiler error parsing with click-to-jump
- LSP diagnostics and completion
- Theme switching (dark/light/retro/classic) and live personalization
- Integrated terminal with in-panel command input
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
cmake --build build-fastener --config Debug -- /m:1
```

Run (Visual Studio generator):

- `.\build-fastener\Debug\Fin.exe`

Release build:

```bash
cmake --build build-fastener --config Release -- /m:1
```

Release executable path:

- `.\build-fastener\Release\Fin.exe`

## Requirements

- Windows 10 or newer
- CMake 3.16+
- C++17 compiler (MSVC or MinGW)
- `clangd` in PATH (for autocomplete/diagnostics)
- `clang++` or `g++` in PATH (for F5 compile/run flow)

## Notes

- Fin is now maintained as a Fastener-only codebase.
- Fastener is consumed as a sibling repo via `add_subdirectory(../Fastener Fastener-build)`.
- App runtime code lives in `src/App` and reusable infrastructure in `src/Core`.
- CMake source lists are centralized in `cmake/FinSources.cmake` for easier maintenance.

## UI Smoke Testing (Windows)

Fin includes a PowerShell UI smoke harness that can:
- launch `Fin.exe`
- click in the window
- send keys (for example `F5`)
- save screenshots

Command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ui_smoke.ps1
```

Default executable path used by the script:

- `build-fastener/Debug/Fin.exe`

Useful options:

```powershell
# Use Release build
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ui_smoke.ps1 `
  -ExePath "build-fastener/Release/Fin.exe"

# Keep the app open after test run
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ui_smoke.ps1 -KeepOpen

# Validate config without launching GUI
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ui_smoke.ps1 -DryRun
```

Action scenario is defined in:
- `scripts/ui/smoke_actions.json`

Action format details:
- `scripts/ui/README.md`

Artifacts are written to:
- `artifacts/ui/<timestamp>/`
