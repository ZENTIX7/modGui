#include "imgui_impl_win32.h"

static HWND g_hwnd = nullptr;

bool ImGui_ImplWin32_Init(void* hwnd) {
    g_hwnd = static_cast<HWND>(hwnd);
    return true;
}

void ImGui_ImplWin32_Shutdown() {
    g_hwnd = nullptr;
}

void ImGui_ImplWin32_NewFrame() {
    if (!g_hwnd) return;
    RECT rect;
    if (GetClientRect(g_hwnd, &rect)) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));
    }
}

LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}
