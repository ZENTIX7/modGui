#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct ImVec2 {
    float x;
    float y;
    ImVec2() : x(0), y(0) {}
    ImVec2(float _x, float _y) : x(_x), y(_y) {}
};

struct ImVec4 {
    float x;
    float y;
    float z;
    float w;
    ImVec4() : x(0), y(0), z(0), w(0) {}
    ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct ImGuiIO {
    ImVec2 DisplaySize;
    float DeltaTime = 1.0f / 60.0f;
    const char* IniFilename = nullptr;
    int ConfigFlags = 0;
};

struct ImGuiStyle {
    float WindowRounding = 0.0f;
    float FrameRounding = 0.0f;
    float ChildRounding = 0.0f;
    float PopupRounding = 0.0f;
    ImVec2 WindowPadding = ImVec2(8, 8);
    ImVec2 ItemSpacing = ImVec2(8, 4);
};

struct ImGuiViewport {
    ImVec2 Pos;
    ImVec2 Size;
};

struct ImDrawList {
    void AddRectFilled(const ImVec2&, const ImVec2&, unsigned int, float = 0.0f, int = 0) {}
    void AddRectFilledMultiColor(const ImVec2&, const ImVec2&, unsigned int, unsigned int, unsigned int, unsigned int) {}
    void AddText(const ImVec2&, unsigned int, const char*) {}
    void AddCircleFilled(const ImVec2&, float, unsigned int, int = 0) {}
    void PathLineTo(const ImVec2&) {}
    void PathStroke(unsigned int, bool = false, float = 1.0f) {}
};

enum ImGuiConfigFlags_ {
    ImGuiConfigFlags_NavEnableKeyboard = 1 << 0
};

enum ImGuiWindowFlags_ {
    ImGuiWindowFlags_None = 0,
    ImGuiWindowFlags_NoDecoration = 1 << 0,
    ImGuiWindowFlags_AlwaysAutoResize = 1 << 1,
    ImGuiWindowFlags_NoSavedSettings = 1 << 2,
    ImGuiWindowFlags_NoMove = 1 << 3,
    ImGuiWindowFlags_NoScrollbar = 1 << 4,
    ImGuiWindowFlags_NoBringToFrontOnFocus = 1 << 5
};

enum ImGuiCond_ {
    ImGuiCond_Always = 1 << 0
};

enum ImGuiCol_ {
    ImGuiCol_Button = 0,
    ImGuiCol_ButtonHovered = 1,
    ImGuiCol_ButtonActive = 2
};

enum ImGuiStyleVar_ {
    ImGuiStyleVar_Alpha = 0
};

enum ImGuiMouseButton_ {
    ImGuiMouseButton_Left = 0
};

enum ImDrawFlags_ {
    ImDrawFlags_RoundCornersTop = 1 << 4
};

struct ImDrawData {};

#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))
#define IM_COL32(R, G, B, A) (((A) << 24) | ((B) << 16) | ((G) << 8) | (R))

namespace ImGui {
    void CreateContext();
    void DestroyContext();
    ImGuiIO& GetIO();
    ImGuiStyle& GetStyle();
    ImGuiViewport* GetMainViewport();
    void StyleColorsDark();
    void NewFrame();
    void Render();
    ImDrawData* GetDrawData();

    void SetNextWindowPos(const ImVec2& pos, int cond = 0, const ImVec2& pivot = ImVec2(0, 0));
    void SetNextWindowSize(const ImVec2& size, int cond = 0);
    void SetNextWindowBgAlpha(float alpha);

    bool Begin(const char* name, bool* p_open = nullptr, int flags = 0);
    void End();
    bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false, int flags = 0);
    void EndChild();

    void SetCursorPos(const ImVec2& local_pos);
    void SetCursorPosY(float local_y);
    void SetCursorScreenPos(const ImVec2& pos);
    ImVec2 GetCursorPos();
    ImVec2 GetContentRegionAvail();
    ImVec2 GetWindowPos();
    ImVec2 GetWindowSize();
    ImDrawList* GetWindowDrawList();

    void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
    void Separator();

    bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool Checkbox(const char* label, bool* v);
    bool InputText(const char* label, char* buf, size_t buf_size, int flags = 0);
    void Text(const char* fmt, ...);
    void TextColored(const ImVec4& col, const char* fmt, ...);
    void ProgressBar(float fraction, const ImVec2& size = ImVec2(0, 0), const char* overlay = nullptr);

    void PushStyleVar(int idx, float val);
    void PopStyleVar(int count = 1);
    void PushStyleColor(int idx, unsigned int col);
    void PopStyleColor(int count = 1);

    bool InvisibleButton(const char* str_id, const ImVec2& size);
    bool IsItemActive();
    bool IsMouseDragging(int button, float lock_threshold = -1.0f);

    float GetTime();
    const char* GetClipboardText();
    unsigned int ColorConvertFloat4ToU32(const ImVec4& in);
}
