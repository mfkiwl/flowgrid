#include "TextBuffer.h"

#include "imgui.h"

#include "Core/WindowsAction.h"
#include "Project/Audio/Faust/FaustAction.h"
#include "Project/TextEditor/TextEditor.h"
#include "UI/UI.h"

static TextEditor editor; // TODO not static

static const Menu FileMenu = {"File", {Action::Faust::File::ShowOpenDialog::MenuItem, Action::Faust::File::ShowSaveDialog::MenuItem}};

TextBuffer::TextBuffer(ComponentArgs &&args, string_view value)
    : PrimitiveField(std::move(args), string(value)) {}

void TextBuffer::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::TextBuffer::Set &a) { Set(a.value); },
    );
}

using namespace ImGui;

void TextBuffer::RenderMenu() const {
    if (BeginMenuBar()) {
        FileMenu.Draw();
        if (BeginMenu("Edit")) {
            MenuItem("Read-only mode", nullptr, &editor.ReadOnly);
            Separator();
            if (MenuItem("Undo", "ALT-Backspace", nullptr, !editor.ReadOnly && editor.CanUndo())) editor.Undo();
            if (MenuItem("Redo", "Ctrl-Y", nullptr, !editor.ReadOnly && editor.CanRedo())) editor.Redo();
            Separator();
            if (MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection())) editor.Copy();
            if (MenuItem("Cut", "Ctrl-X", nullptr, !editor.ReadOnly && editor.HasSelection())) editor.Cut();
            if (MenuItem("Delete", "Del", nullptr, !editor.ReadOnly && editor.HasSelection())) editor.Delete();
            if (MenuItem("Paste", "Ctrl-V", nullptr, !editor.ReadOnly && GetClipboardText() != nullptr)) editor.Paste();
            Separator();
            if (MenuItem("Select all", nullptr, nullptr)) {
                editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));
            }
            EndMenu();
        }

        if (BeginMenu("View")) {
            if (BeginMenu("Palette")) {
                if (MenuItem("Mariana palette")) editor.SetPalette(TextEditor::GetMarianaPalette());
                if (MenuItem("Dark palette")) editor.SetPalette(TextEditor::GetDarkPalette());
                if (MenuItem("Light palette")) editor.SetPalette(TextEditor::GetLightPalette());
                if (MenuItem("Retro blue palette")) editor.SetPalette(TextEditor::GetRetroBluePalette());
                EndMenu();
            }
            // todo show checkbox showing debug window visibility. First, components need access to `Windows::VisibleComponents`.
            //   Spiritualy, `Windows::VisibleComponents` should be a static `Component` field.
            if (MenuItem("Toggle debug window", nullptr, false, true)) Action::Windows::ToggleDebug{Debug.Id}.q();
            EndMenu();
        }
        EndMenuBar();
    }
}

void TextBuffer::Render() const {
    RenderMenu();

    static bool initialized = false;
    static auto lang = TextEditor::LanguageDefT::CPlusPlus();

    if (!initialized) {
        initialized = true;
        editor.SetLanguageDefinition(lang);
    }
    auto cpos = editor.GetCursorPosition();

    const string editing_file = "no file";
    Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.Line + 1, cpos.Column + 1, editor.GetTotalLines(),
        editor.Overwrite ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageDefinitionName(),
        editing_file.c_str()
    );

    PushFont(Ui.Fonts.FixedWidth);
    editor.Render("TextEditor");
    PopFont();

    const string text = editor.GetText();
    if (editor.TextChanged) {
        Action::TextBuffer::Set{Path, text}.q();
    } else if (Value != text) {
        // TODO this is not the usual immediate-mode case. Only set text if the text changed.
        //   Really what I want is to incorporate the TextEditor undo/redo system into the FlowGrid system.
        editor.SetText(Value);
    }
}

void TextBuffer::RenderDebug() const {
    editor.DebugPanel();
}
