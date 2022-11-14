#pragma once

#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h"
#include "implot_internal.h"

#include "../Helper/Time.h"

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

    bool is_widget_gesturing{};
};

extern UIContext UiContext; // Created in `main.cpp`
