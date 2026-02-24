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

## Source List Management

- Source lists are centralized in `cmake/FinSources.cmake`.
- `CMakeLists.txt` defines target wiring and global build settings.
