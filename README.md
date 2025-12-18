# Fin - The Fast IDE

A lightweight, fast C++ IDE built with ImGui and OpenGL. Features a modern tabbed interface, real-time code editing, compilation, and execution all in one place.

## Features

- **Multi-tab Editor**: Open and manage multiple C++ files simultaneously
- **Syntax Highlighting**: Color-coded source code for better readability
- **Auto-Closing Brackets**: Automatic insertion of matching brackets and quotes
- **File Explorer**: Built-in file browser for quick file navigation
- **Compilation & Execution**: Compile and run C++ programs with F5 (using g++)
- **Error Highlighting**: Parse and display compilation errors with clickable navigation
- **Dockable Interface**: Customizable window layout with docking support
- **Zoom Controls**: Adjust text size with Ctrl + Mouse Wheel
- **Session Persistence**: Remembers open files and window state between sessions

## System Requirements

- **Windows 10 or later** (currently Windows-focused)
- **Git** for cloning the repository
- **CMake 3.14 or later** for building
- **Visual Studio 2019+ or MinGW-w64** for compilation
- **g++** for C++ compilation (included with MinGW)
- **OpenGL 3.0+ capable GPU**

## Prerequisites

Before building, ensure you have the following tools installed:

1. **CMake**: Download from [cmake.org](https://cmake.org/download/)
2. **Visual Studio** or **MinGW-w64**:
   - Option A: Install Visual Studio 2019+ with C++ tools
   - Option B: Install [MinGW-w64](https://www.mingw-w64.org/) for g++
3. **Git**: Download from [git-scm.com](https://git-scm.com/)

## Installation

### Step 1: Clone the Repository

```bash
git clone https://github.com/yourusername/Fin.git
cd Fin
```

### Step 2: Create Build Directory

```bash
mkdir build
cd build
```

### Step 3: Generate Build Files with CMake

**For Visual Studio 2019+ (64-bit):**
```bash
cmake .. -G "Visual Studio 16 2019" -A x64
```

**For Visual Studio 2022 (64-bit):**
```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

**For MinGW with Makefiles:**
```bash
cmake .. -G "Unix Makefiles"
```

### Step 4: Build the Project

**With Visual Studio:**
```bash
cmake --build . --config Release
```

**With Make:**
```bash
make
```

Or manually open `Fin.sln` in Visual Studio and build from there.

### Step 5: Run the Application

The executable will be generated in the build directory:

**From Release build:**
```bash
Release\Fin.exe
```

**From Debug build:**
```bash
Debug\Fin.exe
```

## Build Output

After successful compilation, the executable `Fin.exe` will be located in:
- `build\Release\Fin.exe` (Release build)
- `build\Debug\Fin.exe` (Debug build)

## Usage

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | Create new file |
| `Ctrl+O` | Open file dialog |
| `Ctrl+S` | Save current file |
| `Ctrl+W` | Close current tab |
| `F5` | Compile and run |
| `Ctrl+Backspace` | Navigate to parent directory |
| `Ctrl+Scroll` | Zoom in/out |

### Compiling C++ Programs

1. Open a `.cpp` file in the editor
2. Press **F5** or select "Buduj" → "Kompiluj i Uruchom" from menu
3. View compilation errors in the "Konsola Wyjscia" (Output Console)
4. Click on error messages to jump to the problematic line

## Project Structure

```
Fin/
├── CMakeLists.txt          # CMake configuration
├── build/                  # Build directory (generated)
├── src/
│   ├── main.cpp           # Main application entry point
│   ├── Utils.cpp/h        # Utility functions
│   ├── EditorTab.h        # Tab management structure
├── testFiles/             # Test C++ files
└── README.md              # This file
```

## Dependencies

The project automatically downloads the following dependencies via CMake FetchContent:

- **GLFW 3.3.8**: Window and input management
- **ImGui (docking branch)**: UI framework
- **ImGuiColorTextEdit**: Syntax-highlighted text editor
- **Portable File Dialogs**: Cross-platform file dialogs

## Troubleshooting

### CMake not found
- Ensure CMake is installed and added to PATH
- Restart your terminal after installing CMake

### G++ not found
- Install MinGW-w64 or ensure Visual Studio is properly installed
- Add g++ to your system PATH

### OpenGL errors
- Update your graphics drivers
- Ensure your GPU supports OpenGL 3.0+

### Build fails on GLFW/ImGui
- Delete the `build` directory and start fresh
- Ensure you have internet connection (CMake downloads dependencies)
- Check that Git is properly installed

### Application doesn't start
- Run from command line to see error messages
- Verify fonts are available at `C:\Windows\Fonts\`
- Check if build was successful (look for `.exe` file)

## Development

### Modifying the UI
- ImGui code is in `src/main.cpp`
- Refer to [ImGui documentation](https://github.com/ocornut/imgui) for UI modifications

### Building in Debug Mode

```bash
cmake --build . --config Debug
```

## License

Check the LICENSE file for project licensing information.

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Submit a pull request

## Support & Issues

If you encounter issues:
1. Check the [Troubleshooting](#troubleshooting) section
2. Verify all prerequisites are installed
3. Delete `build` directory and rebuild from scratch
4. Open an issue on GitHub with detailed error messages

## Changelog

### Version 1.0
- Initial release
- Multi-tab editor with syntax highlighting
- File explorer integration
- C++ compilation and execution
- Error highlighting and navigation
- Dockable UI layout
- Session persistence
