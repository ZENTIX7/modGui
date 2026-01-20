#include "imgui.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <windows.h>

namespace {
    ImGuiIO g_io;
    ImGuiStyle g_style;
    ImGuiViewport g_viewport;
    ImDrawList g_draw_list;
    ImDrawData g_draw_data;
    ImVec2 g_cursor_pos(0, 0);
    ImVec2 g_window_pos(0, 0);
    ImVec2 g_window_size(0, 0);
    bool g_item_active = false;
    std::chrono::steady_clock::time_point g_start_time;
    std::string g_clipboard_cache;
}

namespace ImGui {
    void CreateContext() {
        g_start_time = std::chrono::steady_clock::now();
    }

    void DestroyContext() {}

    ImGuiIO& GetIO() {
        return g_io;
    }

    ImGuiStyle& GetStyle() {
        return g_style;
    }

    ImGuiViewport* GetMainViewport() {
        return &g_viewport;
    }

    void StyleColorsDark() {}

    void NewFrame() {
        g_item_active = false;
    }

    void Render() {}

    ImDrawData* GetDrawData() {
        return &g_draw_data;
    }

    void SetNextWindowPos(const ImVec2& pos, int, const ImVec2&) {
        g_window_pos = pos;
    }

    void SetNextWindowSize(const ImVec2& size, int) {
        g_window_size = size;
        g_viewport.Pos = g_window_pos;
        g_viewport.Size = size;
    }

    void SetNextWindowBgAlpha(float) {}

    bool Begin(const char*, bool*, int) {
        g_cursor_pos = ImVec2(0, 0);
        return true;
    }

    void End() {}

    bool BeginChild(const char*, const ImVec2&, bool, int) {
        g_cursor_pos = ImVec2(0, 0);
        return true;
    }

    void EndChild() {}

    void SetCursorPos(const ImVec2& local_pos) {
        g_cursor_pos = local_pos;
    }

    void SetCursorPosY(float local_y) {
        g_cursor_pos.y = local_y;
    }

    void SetCursorScreenPos(const ImVec2& pos) {
        g_cursor_pos = pos;
    }

    ImVec2 GetCursorPos() {
        return g_cursor_pos;
    }

    ImVec2 GetContentRegionAvail() {
        return g_window_size;
    }

    ImVec2 GetWindowPos() {
        return g_window_pos;
    }

    ImVec2 GetWindowSize() {
        return g_window_size;
    }

    ImDrawList* GetWindowDrawList() {
        return &g_draw_list;
    }

    void SameLine(float, float) {}

    void Separator() {}

    bool Button(const char*, const ImVec2&) {
        return false;
    }

    bool Checkbox(const char*, bool*) {
        return false;
    }

    bool InputText(const char*, char*, size_t, int) {
        return false;
    }

    void Text(const char* fmt, ...) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
    }

    void TextColored(const ImVec4&, const char* fmt, ...) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
    }

    void ProgressBar(float, const ImVec2&, const char*) {}

    void PushStyleVar(int, float) {}

    void PopStyleVar(int) {}

    void PushStyleColor(int, unsigned int) {}

    void PopStyleColor(int) {}

    bool InvisibleButton(const char*, const ImVec2&) {
        g_item_active = false;
        return false;
    }

    bool IsItemActive() {
        return g_item_active;
    }

    bool IsMouseDragging(int, float) {
        return false;
    }

    float GetTime() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_start_time);
        return elapsed.count();
    }

    const char* GetClipboardText() {
        g_clipboard_cache.clear();
        if (!OpenClipboard(nullptr)) {
            return nullptr;
        }
        HANDLE data = GetClipboardData(CF_TEXT);
        if (!data) {
            CloseClipboard();
            return nullptr;
        }
        char* text = static_cast<char*>(GlobalLock(data));
        if (text) {
            g_clipboard_cache = text;
            GlobalUnlock(data);
        }
        CloseClipboard();
        return g_clipboard_cache.empty() ? nullptr : g_clipboard_cache.c_str();
    }

    unsigned int ColorConvertFloat4ToU32(const ImVec4& in) {
        auto to_byte = [](float v) {
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            return static_cast<unsigned int>(v * 255.0f + 0.5f);
        };
        unsigned int r = to_byte(in.x);
        unsigned int g = to_byte(in.y);
        unsigned int b = to_byte(in.z);
        unsigned int a = to_byte(in.w);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
}
