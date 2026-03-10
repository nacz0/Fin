# Architecture

## Modules

- `src/Core`: platform/infrastructure services (terminal process, LSP client, config, file I/O, compiler output parsing).
- `src/App`: application orchestration and UI-level helpers.
- `src/App/Panels`: renderers for dockable UI panels.

## Build Targets

- `fin_core` (static library): core services from `src/Core`.
- `fin_app` (static library): application and panel layer from `src/App`.
- `Fin` (executable): entry point (`src/main.cpp`) linked against `fin_app`.

## Dependency Direction

- `Fin` -> `fin_app` -> `fin_core`
- `fin_app` also links `fastener` for UI rendering.
- `thirdparty/json.hpp` is used by the LSP layer.

## Docking Integration

- Fin enables dock preview rendering by calling `fst::RenderDockPreview(ctx)` in `src/App/FinApp.cpp`.
- Preview overlays and dock target arrows are rendered inside Fastener (`../Fastener/src/widgets/dock_preview.cpp`).

## Input Ownership Rules

- Terminal keyboard input is handled only when the `terminal_console_input` widget is focused (`src/App/Panels/TerminalPanel.cpp`).
- Terminal history/output is rendered readonly; typing applies only to the current command line.
- Commands sent to `cmd.exe` use CRLF (`\r\n`) line endings (`src/Core/Terminal.cpp`).
- Editor keyboard handling must stay scoped to focused editor widgets (no global text input routing).

## Source List Management

- Source lists are centralized in `cmake/FinSources.cmake`.
- `CMakeLists.txt` defines target wiring and global build settings.
