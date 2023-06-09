#include "UI.h"

#include "imgui.h"
#include "implot.h"

#include "imgui_impl_opengl3.h" // TODO vulkan
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "App/ImGuiSettings.h"
#include "App/Style/Style.h"
#include "Core/Store/StoreAction.h"

#ifdef TRACING_ENABLED
#include <Tracy.hpp>
#endif

using fg::style;

static SDL_Window *Window = nullptr;
static SDL_GLContext GlContext{};

UIContext::UIContext() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0) throw std::runtime_error(SDL_GetError());

#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;

    Window = SDL_CreateWindowWithPosition("FlowGrid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    GlContext = SDL_GL_CreateContext(Window);

    SDL_GL_MakeCurrent(Window, GlContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving. We handle this manually.

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.FontAllowUserScaling = true;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(Window, GlContext);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    io.FontGlobalScale = style.ImGui.FontScale / FontAtlasScale;
    Fonts.Main = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16 * FontAtlasScale);
    Fonts.FixedWidth = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Cousine-Regular.ttf", 15 * FontAtlasScale);
    io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/ProggyClean.ttf", 14 * FontAtlasScale);
}

UIContext::~UIContext() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    SDL_GL_DeleteContext(GlContext);
    SDL_DestroyWindow(Window);
    SDL_Quit();
}

void PrepareFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void RenderFrame() {
    ImGui::Render();

    const auto &io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(Window);
}

bool UIContext::Tick(const Drawable &app) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(Window))) {
            return false;
        }
    }

    // Check if new UI settings need to be applied.
    auto &flags = UpdateFlags;
    if (flags != UIContext::Flags_None) {
        if (flags & UIContext::Flags_ImGuiSettings) imgui_settings.Update(ImGui::GetCurrentContext());
        if (flags & UIContext::Flags_ImGuiStyle) style.ImGui.Update(ImGui::GetCurrentContext());
        if (flags & UIContext::Flags_ImPlotStyle) style.ImPlot.Update(ImPlot::GetCurrentContext());
        flags = UIContext::Flags_None;
    }

    auto &io = ImGui::GetIO();

    static int PrevFontIndex = 0;
    static float PrevFontScale = 1.0;
    if (PrevFontIndex != style.ImGui.FontIndex) {
        io.FontDefault = io.Fonts->Fonts[style.ImGui.FontIndex];
        PrevFontIndex = style.ImGui.FontIndex;
    }
    if (PrevFontScale != style.ImGui.FontScale) {
        io.FontGlobalScale = style.ImGui.FontScale / FontAtlasScale;
        PrevFontScale = style.ImGui.FontScale;
    }

    PrepareFrame();
    app.Draw(); // All application content drawing, initial dockspace setup, keyboard shortcuts.
    RenderFrame();

    if (io.WantSaveIniSettings) {
        // ImGui sometimes sets this flags when settings have not, in fact, changed.
        // E.g. if you click and hold a window-resize, it will set this every frame, even if the cursor is still (no window size change).
        const auto &patch = imgui_settings.CreatePatch(ImGui::GetCurrentContext());
        if (!patch.Empty()) Action::Store::ApplyPatch{patch}.q();
        io.WantSaveIniSettings = false;
    }

#ifdef TRACING_ENABLED
    FrameMark;
#endif

    return true;
}
