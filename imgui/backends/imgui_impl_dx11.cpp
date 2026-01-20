#include "imgui_impl_dx11.h"

bool ImGui_ImplDX11_Init(ID3D11Device*, ID3D11DeviceContext*) {
    return true;
}

void ImGui_ImplDX11_Shutdown() {}

void ImGui_ImplDX11_NewFrame() {}

void ImGui_ImplDX11_RenderDrawData(ImDrawData*) {}
