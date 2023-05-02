#pragma once

struct ImGuiContext;
struct ImPlotContext;
struct ImFont;

struct UIContext {
    enum Flags_ {
        Flags_None = 0,
        Flags_ImGuiSettings = 1 << 0,
        Flags_ImGuiStyle = 1 << 1,
        Flags_ImPlotStyle = 1 << 2,
    };
    using Flags = int;

    struct Fonts {
        ImFont *Main{nullptr};
        ImFont *FixedWidth{nullptr};
    };

    void WidgetGestured();

    ImGuiContext *ImGui{nullptr};
    ImPlotContext *ImPlot{nullptr};
    Fonts Fonts{};

    bool IsWidgetGesturing{};
    Flags ApplyFlags = Flags_None;
};

UIContext CreateUi();
void TickUi();
void DestroyUi();

extern UIContext UiContext; // Created in `main.cpp`
