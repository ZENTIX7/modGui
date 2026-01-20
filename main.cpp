#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <dwmapi.h>
#include <winhttp.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comdlg32.lib")

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kTitleBarHeight = 48.0f;

static float ClampFloat(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide.data(), size);
    return wide;
}

static std::string WideToUtf8(const std::wstring& str) {
    if (str.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    return utf8;
}

static std::string UrlEncode(const std::string& value) {
    static const char* kHex = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[c >> 4]);
            encoded.push_back(kHex[c & 0x0F]);
        }
    }
    return encoded;
}

struct Toast {
    std::string message;
    ImVec4 color;
    float start_time = 0.0f;
    float duration = 3.0f;
};

static void AddToast(std::vector<Toast>& toasts, const std::string& message, const ImVec4& color) {
    if (toasts.size() >= 4) {
        toasts.erase(toasts.begin());
    }
    Toast toast;
    toast.message = message;
    toast.color = color;
    toast.start_time = static_cast<float>(ImGui::GetTime());
    toast.duration = 3.0f;
    toasts.push_back(toast);
}

static void DrawToasts(std::vector<Toast>& toasts) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 base_pos(viewport->Pos.x + viewport->Size.x - 20.0f, viewport->Pos.y + 20.0f);
    float y_offset = 0.0f;
    float now = static_cast<float>(ImGui::GetTime());
    for (int i = static_cast<int>(toasts.size()) - 1; i >= 0; --i) {
        Toast& toast = toasts[i];
        float t = (now - toast.start_time) / toast.duration;
        if (t >= 1.0f) {
            toasts.erase(toasts.begin() + i);
            continue;
        }
        float alpha = 1.0f;
        if (t < 0.1f) {
            alpha = t / 0.1f;
        } else if (t > 0.85f) {
            alpha = (1.0f - t) / 0.15f;
        }
        alpha = ClampFloat(alpha, 0.0f, 1.0f);
        ImGui::SetNextWindowBgAlpha(0.85f * alpha);
        ImGui::SetNextWindowPos(ImVec2(base_pos.x, base_pos.y + y_offset), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin(("toast_" + std::to_string(i)).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(toast.color.x, toast.color.y, toast.color.z, alpha), "%s", toast.message.c_str());
        y_offset += ImGui::GetWindowSize().y + 8.0f;
        ImGui::End();
    }
}

enum class ScreenState {
    Login,
    Loading,
    Main
};

struct VerifyResult {
    bool success = false;
    bool network_error = false;
    std::string status_message;
};

static const wchar_t* kVerifyHost = L"example.com";
static const INTERNET_PORT kVerifyPort = 443;
static const bool kVerifyUseHttps = true;
static const char* kVerifyPathPrefix = "/verify/v1/nitrosdk?key=";

static VerifyResult VerifyKeyOnline(const std::string& key) {
    VerifyResult result;
    std::wstring host(kVerifyHost);
    std::string query = std::string(kVerifyPathPrefix) + UrlEncode(key);
    std::wstring path = Utf8ToWide(query);

    HINTERNET session = WinHttpOpen(L"ModGui/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.network_error = true;
        result.status_message = "Network error: WinHttpOpen failed";
        return result;
    }

    HINTERNET connect = WinHttpConnect(session, host.c_str(), kVerifyPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        result.network_error = true;
        result.status_message = "Network error: WinHttpConnect failed";
        return result;
    }

    DWORD flags = kVerifyUseHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.network_error = true;
        result.status_message = "Network error: WinHttpOpenRequest failed";
        return result;
    }

    BOOL sent = WinHttpSendRequest(request,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0,
                                   0, 0);
    if (!sent) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.network_error = true;
        result.status_message = "Network error: WinHttpSendRequest failed";
        return result;
    }

    BOOL received = WinHttpReceiveResponse(request, nullptr);
    if (!received) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.network_error = true;
        result.status_message = "Network error: WinHttpReceiveResponse failed";
        return result;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

    std::string body;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::string buffer(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read) || read == 0) {
            break;
        }
        buffer.resize(read);
        body += buffer;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    result.status_message = "HTTP " + std::to_string(status_code);
    if (body.find("\"valid\":true") != std::string::npos ||
        body.find("\"success\":true") != std::string::npos ||
        body == "true") {
        result.success = true;
    } else {
        result.success = false;
    }
    return result;
}

struct AppState {
    ScreenState current = ScreenState::Login;
    ScreenState target = ScreenState::Login;
    float transition = 1.0f;
    float transition_start = 0.0f;
    float loading_progress = 0.0f;
    float loading_start = 0.0f;

    char key_input[128] = "";
    bool show_key = false;
    bool remember_me = false;
    bool verifying = false;
    std::string status_text;

    std::mutex verify_mutex;
    VerifyResult verify_result;
    std::atomic<bool> verify_ready{false};

    std::wstring selected_path;
    std::wstring selected_name;
    PROCESS_INFORMATION target_process{};
    bool has_process = false;

    std::vector<Toast> toasts;
};

static void StartTransition(AppState& state, ScreenState next) {
    state.target = next;
    state.transition = 0.0f;
    state.transition_start = static_cast<float>(ImGui::GetTime());
}

static void DrawSpinner(ImDrawList* draw_list, const ImVec2& center, float radius, float thickness, float t) {
    int num_segments = 30;
    float start = t * 4.0f;
    float end = start + kPi * 1.5f;
    for (int i = 0; i < num_segments; ++i) {
        float a0 = start + (end - start) * (static_cast<float>(i) / num_segments);
        float a1 = start + (end - start) * (static_cast<float>(i + 1) / num_segments);
        float ca0 = static_cast<float>(std::cos(a0));
        float sa0 = static_cast<float>(std::sin(a0));
        float ca1 = static_cast<float>(std::cos(a1));
        float sa1 = static_cast<float>(std::sin(a1));
        draw_list->PathLineTo(ImVec2(center.x + ca0 * radius, center.y + sa0 * radius));
        draw_list->PathLineTo(ImVec2(center.x + ca1 * radius, center.y + sa1 * radius));
    }
    draw_list->PathStroke(IM_COL32(120, 180, 255, 200), false, thickness);
}

static bool IsProcessRunning(const PROCESS_INFORMATION& pi) {
    if (!pi.hProcess) return false;
    DWORD wait = WaitForSingleObject(pi.hProcess, 0);
    return wait == WAIT_TIMEOUT;
}

static std::wstring GetFileNameFromPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

static bool OpenExeDialog(std::wstring& out_path) {
    wchar_t buffer[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        out_path = buffer;
        return true;
    }
    return false;
}

static bool LaunchTarget(const std::wstring& path, PROCESS_INFORMATION& out_pi) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::wstring command = L"\"" + path + L"\"";
    BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (ok) {
        out_pi = pi;
        return true;
    }
    return false;
}

static void DrawTitleBar(HWND hwnd, const ImVec2& window_pos, const ImVec2& window_size, const ImVec4& accent) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 title_pos = window_pos;
    ImVec2 title_size(window_size.x, kTitleBarHeight);
    draw_list->AddRectFilled(title_pos, ImVec2(title_pos.x + title_size.x, title_pos.y + title_size.y),
                             IM_COL32(15, 18, 24, 255), 12.0f, ImDrawFlags_RoundCornersTop);
    draw_list->AddText(ImVec2(title_pos.x + 18.0f, title_pos.y + 14.0f),
                       IM_COL32(200, 230, 255, 255), "LITHIUM.RIP");
    draw_list->AddCircleFilled(ImVec2(title_pos.x + 6.0f, title_pos.y + 22.0f), 6.0f,
                               ImGui::ColorConvertFloat4ToU32(accent));

    ImGui::SetCursorScreenPos(ImVec2(title_pos.x + window_size.x - 60.0f, title_pos.y + 12.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 50, 60, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 80, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(90, 100, 115, 255));
    if (ImGui::Button("-", ImVec2(20, 20))) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    if (ImGui::Button("x", ImVec2(20, 20))) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    ImGui::PopStyleColor(3);
}

static void DrawBackgroundGradient(const ImVec2& pos, const ImVec2& size) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    unsigned int col_top = IM_COL32(10, 12, 18, 255);
    unsigned int col_bottom = IM_COL32(5, 8, 12, 255);
    draw_list->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                       col_top, col_top, col_bottom, col_bottom);
}

static void DrawDebugOverlay(const AppState& state, bool transition_active, float transition_t, float alpha_out, float alpha_in, const ImGuiIO& io) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340, 180), ImGuiCond_Always);
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("IMGUI DEBUG: YOU SHOULD SEE THIS");
    ImGui::Text("io.DisplaySize: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::Text("io.DeltaTime: %.4f", io.DeltaTime);
    ImGui::Text("current screen: %d", static_cast<int>(state.current));
    ImGui::Text("transition.active: %s", transition_active ? "true" : "false");
    ImGui::Text("transition.t: %.2f", transition_t);
    ImGui::Text("alpha out/in: %.2f / %.2f", alpha_out, alpha_in);
    ImGui::End();
}

static void DrawLoginScreen(AppState& state) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 card_size(380.0f, 320.0f);
    ImVec2 card_pos((avail.x - card_size.x) * 0.5f, (avail.y - card_size.y) * 0.5f);
    if (card_pos.x < 0) card_pos.x = 0;
    if (card_pos.y < 0) card_pos.y = 0;

    ImGui::SetCursorPos(card_pos);
    ImGui::BeginChild("login_panel", card_size, true);
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "SIGN IN");
    ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "Best UD Cheats since 2024");
    ImGui::Separator();
    ImGui::Text("License Key");
    ImGuiInputTextFlags flags = state.show_key ? 0 : ImGuiInputTextFlags_Password;
    ImGui::InputText("##key", state.key_input, IM_ARRAYSIZE(state.key_input), flags);
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        if (const char* clip = ImGui::GetClipboardText()) {
            strncpy_s(state.key_input, clip, IM_ARRAYSIZE(state.key_input) - 1);
        }
    }
    ImGui::Checkbox("Show", &state.show_key);
    if (ImGui::Button("Sign In", ImVec2(-1, 0))) {
        if (!state.verifying) {
            state.verifying = true;
            state.status_text = "Verifying...";
            std::string key = state.key_input;
            std::thread([&state, key]() {
                VerifyResult res = VerifyKeyOnline(key);
                std::lock_guard<std::mutex> lock(state.verify_mutex);
                state.verify_result = res;
                state.verify_ready.store(true);
            }).detach();
        }
    }
    ImGui::Checkbox("Remember me", &state.remember_me);
    if (!state.status_text.empty()) {
        ImVec4 status_color = state.status_text == "Key declined" ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f)
            : ImVec4(0.7f, 0.8f, 0.9f, 1.0f);
        ImGui::TextColored(status_color, "%s", state.status_text.c_str());
    }
    ImGui::EndChild();
}

static void DrawLoadingScreen(AppState& state, float now) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 card_size(280.0f, 260.0f);
    ImVec2 card_pos((avail.x - card_size.x) * 0.5f, (avail.y - card_size.y) * 0.5f);
    if (card_pos.x < 0) card_pos.x = 0;
    if (card_pos.y < 0) card_pos.y = 0;

    ImGui::SetCursorPos(card_pos);
    ImGui::BeginChild("loading_card", card_size, true);
    ImVec2 card_pos_abs = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 card_size_abs = ImGui::GetContentRegionAvail();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 spinner_center(card_pos_abs.x + card_size_abs.x * 0.5f, card_pos_abs.y + 90.0f);
    DrawSpinner(draw_list, spinner_center, 32.0f, 4.0f, now);
    ImGui::SetCursorPosY(140);
    ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Loading");
    float elapsed = now - state.loading_start;
    state.loading_progress = ClampFloat(elapsed / 2.5f, 0.0f, 1.0f);
    ImGui::ProgressBar(state.loading_progress, ImVec2(-1, 8));
    if (state.loading_progress >= 1.0f && state.transition >= 1.0f) {
        StartTransition(state, ScreenState::Main);
    }
    ImGui::EndChild();
}

static void DrawMainScreen(AppState& state) {
    ImVec2 content = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("sidebar", ImVec2(140, content.y), true);
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "Main");
    ImGui::Separator();
    ImGui::Text("Dashboard");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("content_panel", ImVec2(0, content.y), false);
    ImGui::BeginChild("target_card", ImVec2(0, 140), true);
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Target");
    ImGui::Separator();
    std::string target_name = state.selected_name.empty() ? "No target selected" : WideToUtf8(state.selected_name);
    ImGui::Text("Selected: %s", target_name.c_str());
    bool running = state.has_process && IsProcessRunning(state.target_process);
    if (running) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "Running (PID %lu)", state.target_process.dwProcessId);
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Not running");
    }
    if (ImGui::Button("Browse...", ImVec2(120, 0))) {
        std::wstring path;
        if (OpenExeDialog(path)) {
            state.selected_path = path;
            state.selected_name = GetFileNameFromPath(path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Launch Target", ImVec2(140, 0))) {
        if (!state.selected_path.empty()) {
            if (state.has_process) {
                CloseHandle(state.target_process.hProcess);
                CloseHandle(state.target_process.hThread);
            }
            PROCESS_INFORMATION pi = {};
            if (LaunchTarget(state.selected_path, pi)) {
                state.target_process = pi;
                state.has_process = true;
            } else {
                AddToast(state.toasts, "Failed to launch target", ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Inject", ImVec2(100, 0))) {
        AddToast(state.toasts, "Injected!", ImVec4(0.2f, 0.9f, 0.3f, 1.0f));
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// DirectX/Win32 globals.
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND g_hWnd = nullptr;

static void CreateRenderTarget() {
    // Create the render target view from the swap chain back buffer.
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
    if (!g_mainRenderTargetView) {
        OutputDebugStringA("RenderTargetView is null after CreateRenderTarget\n");
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                     createDeviceFlags, featureLevelArray, 1,
                                     D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                     &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) {
        return false;
    }

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST: {
        // Custom drag region for borderless window.
        POINTS pt = MAKEPOINTS(lParam);
        POINT client_pt = { pt.x, pt.y };
        ScreenToClient(hWnd, &client_pt);
        if (client_pt.y >= 0 && client_pt.y < static_cast<LONG>(kTitleBarHeight)) {
            return HTCAPTION;
        }
        break;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            // Recreate render target after swapchain resize.
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Gray/blank screen was caused by not actually rendering ImGui windows when alpha
    // transitions effectively zeroed them out. This version enforces a debug overlay
    // and disables transitions until UI is visible.
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                      _T("ModGuiWindow"), nullptr };
    RegisterClassEx(&wc);

    // Create a borderless window (no OS title bar).
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("ModGui"),
                             WS_POPUP, 100, 100, 540, 660,
                             nullptr, nullptr, wc.hInstance, nullptr);
    g_hWnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.ChildRounding = 10.0f;
    style.PopupRounding = 8.0f;
    style.WindowPadding = ImVec2(20, 20);
    style.ItemSpacing = ImVec2(12, 12);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    AppState state;
    bool done = false;
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (!done) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (state.verify_ready.load()) {
            std::lock_guard<std::mutex> lock(state.verify_mutex);
            state.verifying = false;
            state.verify_ready.store(false);
            if (state.verify_result.network_error) {
                state.status_text = state.verify_result.status_message;
                AddToast(state.toasts, state.verify_result.status_message, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            } else if (state.verify_result.success) {
                state.status_text = "Key accepted";
                AddToast(state.toasts, "Key accepted", ImVec4(0.3f, 0.9f, 0.4f, 1.0f));
                StartTransition(state, ScreenState::Loading);
                state.loading_start = static_cast<float>(ImGui::GetTime());
                state.loading_progress = 0.0f;
            } else {
                state.status_text = "Key declined";
                AddToast(state.toasts, "Key declined", ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            }
        }

        // Correct ImGui frame loop order.
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Force transition inactive until UI is confirmed visible.
        bool transition_active = false;
        float transition_t = 1.0f;
        float alpha_out = 1.0f;
        float alpha_in = 1.0f;

        // DEBUG WINDOW (always visible)
        DrawDebugOverlay(state, transition_active, transition_t, alpha_out, alpha_in, io);

        // MAIN UI
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        DrawBackgroundGradient(ImGui::GetWindowPos(), ImGui::GetWindowSize());

        ImVec4 accent(0.25f, 0.55f, 0.95f, 1.0f);
        DrawTitleBar(hwnd, ImGui::GetWindowPos(), ImGui::GetWindowSize(), accent);

        ImGui::SetCursorPos(ImVec2(0, kTitleBarHeight + 10.0f));
        ImGui::BeginChild("Content", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

        // Force login screen to draw for visibility checks.
        DrawLoginScreen(state);

        ImGui::EndChild();
        DrawToasts(state.toasts);
        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.03f, 0.04f, 0.06f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    if (state.has_process) {
        CloseHandle(state.target_process.hProcess);
        CloseHandle(state.target_process.hThread);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
