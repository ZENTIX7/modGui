#pragma once
#include <windows.h>
#include "../imgui.h"

bool ImGui_ImplWin32_Init(void* hwnd);
void ImGui_ImplWin32_Shutdown();
void ImGui_ImplWin32_NewFrame();
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
