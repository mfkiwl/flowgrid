#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h" // TODO metal
#include "draw.h"
#include "context.h"
#include "windows/faust_editor.h"
#include "windows/show_window.h"
#include "imgui_internal.h"

struct InputTextCallback_UserData {
    std::string *Str;
    ImGuiInputTextCallback ChainCallback;
    void *ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    auto *user_data = (InputTextCallback_UserData *) data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        std::string *str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char *) str->c_str();
    } else if (user_data->ChainCallback) {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

bool InputTextMultiline(const char *label, std::string *str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data{str, callback, user_data};
    return ImGui::InputTextMultiline(label, (char *) str->c_str(), str->capacity() + 1, ImVec2(0, 0), flags, InputTextCallback, &cb_user_data);
}

struct DrawContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

void new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

DrawContext create_draw_context() {
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    context.glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    auto window_flags = (SDL_WindowFlags) (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    DrawContext draw_context;
    draw_context.window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        window_flags);
    draw_context.gl_context = SDL_GL_CreateContext(draw_context.window);

    return draw_context;
}

void load_fonts() {
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
}

void setup(DrawContext &dc) {
    SDL_GL_MakeCurrent(dc.window, dc.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    // Disable ImGui's .ini file saving. We handle this manually.
    io.IniFilename = nullptr;
    // However, since the default ImGui behavior is to write to disk (to the .ini file) when the ini state is marked dirty,
    // it buffers marking dirty (`g.IO.WantSaveIniSettings = true`) with a `io.IniSavingRate` timer (which is 5s by default).
    // We want this to be a very small value, since we want to create actions for the undo stack as soon after a user action
    // as possible.
    // TODO closing windows or changing their dockspace should be independent actions, but resize events can get rolled into
    //  the next action?
    io.IniSavingRate = 0.25;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(dc.window, dc.gl_context);
    ImGui_ImplOpenGL3_Init(dc.glsl_version);

    load_fonts();
}

void teardown(DrawContext &dc) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(dc.gl_context);
    SDL_DestroyWindow(dc.window);
    SDL_Quit();
}

void render(DrawContext &dc, const Color &clear_color) {
    ImGui::Render();
    glViewport(0, 0, (int) dc.io.DisplaySize.x, (int) dc.io.DisplaySize.y);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(dc.window);
}

FaustEditor faust_editor{};

// Usually this state management happens in `show_window`, but the demo window doesn't expose
// all its window state handling like we do with internal windows.
// Thus, only the demo window's visibility state is part of the undo stack
// (whereas with internal windows, other things like the collapsed state are considered undoable events).
void draw_demo_window() {
    static const std::string demo_window_name{"Demo"};
    const auto &w = s.ui.windows.at(demo_window_name);
    auto &mutable_w = ui_s.ui.windows[demo_window_name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{demo_window_name});
    if (w.visible) ImGui::ShowDemoWindow(&mutable_w.visible);
}

bool open = true;

// TODO see https://github.com/ocornut/imgui/issues/4033 for an example in getting INI settings into memory
//  and using them to store multiple layouts
//  also see https://github.com/ocornut/imgui/issues/4294#issuecomment-874720489 for how to manage
//  layout saving/loading manually
// TODO see https://github.com/ocornut/imgui/issues/2109#issuecomment-426204357
//  for how to programmatically set up a default layout

void draw_frame() {
    // Adapted from `imgui_demo::ShowExampleAppDockSpace`
    // More docking info at https://github.com/ocornut/imgui/issues/2109
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("FlowGrid", &open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);

    auto dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id);

    // TODO not working. https://github.com/ocornut/imgui/issues/2414 might be a good example
//    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
//        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
//        ImGui::DockBuilderAddNode(dockspace_id, 0); // Add empty node
//        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);
//
//        ImGuiID dock_main_id = dockspace_id; // This variable will track the document node, however we are not using it here as we aren't docking anything into it.
//        ImGuiID dock_id_prop = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
//        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, nullptr, &dock_main_id);
//
//        ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
//        ImGui::DockBuilderDockWindow("Properties", dock_id_prop);
//        ImGui::DockBuilderDockWindow("Mesh", dock_id_prop);
//        ImGui::DockBuilderDockWindow("Extra", dock_id_prop);
//        ImGui::DockBuilderFinish(dockspace_id);
//    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            bool opt_placeholder;
            ImGui::MenuItem("Placeholder", nullptr, &opt_placeholder);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    draw_demo_window();
    show_window("Faust", faust_editor);

    ImGui::Begin("Controls");
    ImGui::BeginDisabled(!c.can_undo());
    if (ImGui::Button("Undo")) { q.enqueue(undo{}); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!c.can_redo());
    if (ImGui::Button("Redo")) { q.enqueue(redo{}); }
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Demo Window", &ui_s.ui.windows["Demo"].visible)) { q.enqueue(toggle_window{"Demo"}); }

    if (ImGui::ColorEdit3("Background color", (float *) &ui_s.ui.colors.clear)) { q.enqueue(set_clear_color{ui_s.ui.colors.clear}); }
    if (ImGui::IsItemActivated()) c.start_gesture();
    if (ImGui::IsItemDeactivatedAfterEdit()) c.end_gesture();

    if (ImGui::Checkbox("Audio thread running", &ui_s.audio.running)) { q.enqueue(toggle_audio_running{}); }
    // TODO allow toggling action_consumer on and off repeatedly
//        q.enqueue(set_action_consumer_running{false});
    if (ImGui::Checkbox("Mute audio", &ui_s.audio.muted)) { q.enqueue(toggle_audio_muted{}); }

    {
//        ImGuiInputTextFlags_NoUndoRedo;
        static auto flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue;
        if (InputTextMultiline("##faust_source", &ui_s.audio.faust.code, flags)) {
            q.enqueue(set_faust_text{ui_s.audio.faust.code});
        }
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        if (!s.audio.faust.error.empty()) ImGui::Text("Faust error:\n%s", s.audio.faust.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::End();
}

int draw() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    auto dc = create_draw_context();
    setup(dc);

    if (!s.ui.ini_settings.empty()) {
        ImGui::LoadIniSettingsFromMemory(s.ui.ini_settings.c_str(), s.ui.ini_settings.size());
    }

    // Main loop
    while (s.ui.running) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                    event.window.windowID == SDL_GetWindowID(dc.window))) {
                q.enqueue(close_application{});
            }
        }

        if (c.new_ini_state) {
            ImGui::LoadIniSettingsFromMemory(s.ui.ini_settings.c_str(), s.ui.ini_settings.size());
            c.new_ini_state = false;
        }

        new_frame();
        draw_frame();
        render(dc, s.ui.colors.clear);

        auto &io = ImGui::GetIO();
        if (io.WantSaveIniSettings) {
            size_t settings_size = 0;
            const char *settings = ImGui::SaveIniSettingsToMemory(&settings_size);
            q.enqueue(set_ini_settings{settings});
            io.WantSaveIniSettings = false;
        }
    }

    faust_editor.destroy();
    teardown(dc);

    return 0;
}
