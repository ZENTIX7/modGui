# ModernLauncher (Visual Studio 2022)

This repository contains a full Visual Studio 2022 C++17 project configured for a Windows subsystem (no console), using Win32 + DirectX11 + Dear ImGui-style files.

## Build Steps
1. Open `ModernLauncher.sln` in Visual Studio 2022.
2. Select **x64** and either **Debug** or **Release**.
3. Build & Run (F5).

## Notes
- The project links against: `d3d11.lib`, `dxgi.lib`, `dwmapi.lib`, `winhttp.lib`, and `comdlg32.lib`.
- The ImGui files provided in `/imgui` are minimal build stubs to keep the template self-contained in this environment. Replace them with the official Dear ImGui sources from https://github.com/ocornut/imgui for full rendering and behavior.
