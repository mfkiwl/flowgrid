#include "Component.h"

#include <format>

#include "imgui_internal.h"

#include "Helper/String.h"
#include "Project/Style/Style.h"
#include "UI/HelpMarker.h"

DebugComponent::DebugComponent(ComponentArgs &&args, float split_ratio)
    : Component(std::move(args)), SplitRatio(split_ratio) {}
DebugComponent::DebugComponent(ComponentArgs &&args, ImGuiWindowFlags flags, Menu &&menu, float split_ratio)
    : Component(std::move(args), flags, std::move(menu)), SplitRatio(split_ratio) {}
DebugComponent::~DebugComponent() {}

Menu::Menu(string_view label, std::vector<const Item> &&items) : Label(label), Items(std::move(items)) {}
Menu::Menu(std::vector<const Item> &&items) : Menu("", std::move(items)) {}
Menu::Menu(std::vector<const Item> &&items, const bool is_main) : Label(""), Items(std::move(items)), IsMain(is_main) {}

Component::Component(Store &store, PrimitiveActionQueuer &primitive_q, const Windows &windows, const fg::Style &style)
    : RootStore(store), PrimitiveQ(primitive_q), gWindows(windows), gStyle(style), Root(this), Parent(nullptr),
      PathSegment(""), Path(RootPath), Name(""), Help(""), ImGuiLabel(""), Id(ImHashStr("", 0, 0)) {
    ById.emplace(Id, this);
    IdByPath.emplace(Path, Id);
}

Component::Component(Component *parent, string_view path_segment, string_view path_prefix_segment, HelpInfo info, ImGuiWindowFlags flags, Menu &&menu)
    : RootStore(parent->RootStore),
      PrimitiveQ(parent->PrimitiveQ),
      gWindows(parent->gWindows),
      gStyle(parent->gStyle),
      Root(parent->Root),
      Parent(parent),
      PathSegment(path_segment),
      Path(path_prefix_segment.empty() ? Parent->Path / PathSegment : Parent->Path / path_prefix_segment / PathSegment),
      Name(info.Name.empty() ? PathSegment.empty() ? "" : StringHelper::PascalToSentenceCase(PathSegment) : info.Name),
      Help(info.Help),
      ImGuiLabel(Name.empty() ? "" : (path_prefix_segment.empty() ? std::format("{}##{}", Name, PathSegment) : std::format("{}##{}/{}", Name, path_prefix_segment, PathSegment))),
      Id(GenerateId(Parent->Id, ImGuiLabel.c_str())),
      WindowMenu(std::move(menu)),
      WindowFlags(flags) {
    ById.emplace(Id, this);
    IdByPath.emplace(Path, Id);
    HelpInfo::ById.emplace(Id, HelpInfo{Name, Help});
    parent->Children.emplace_back(this);
}

Component::Component(ComponentArgs &&args)
    : Component(std::move(args.Parent), std::move(args.PathSegment), std::move(args.PathSegmentPrefix), HelpInfo::Parse(std::move(args.MetaStr)), ImGuiWindowFlags_None, Menu{{}}) {}

Component::Component(ComponentArgs &&args, ImGuiWindowFlags flags)
    : Component(std::move(args.Parent), std::move(args.PathSegment), std::move(args.PathSegmentPrefix), HelpInfo::Parse(std::move(args.MetaStr)), flags, Menu{{}}) {}
Component::Component(ComponentArgs &&args, Menu &&menu)
    : Component(std::move(args.Parent), std::move(args.PathSegment), std::move(args.PathSegmentPrefix), HelpInfo::Parse(std::move(args.MetaStr)), ImGuiWindowFlags_None, std::move(menu)) {}
Component::Component(ComponentArgs &&args, ImGuiWindowFlags flags, Menu &&menu)
    : Component(std::move(args.Parent), std::move(args.PathSegment), std::move(args.PathSegmentPrefix), HelpInfo::Parse(std::move(args.MetaStr)), flags, std::move(menu)) {}

Component::~Component() {
    if (Parent) std::erase_if(Parent->Children, [this](const auto *child) { return child == this; });
    ById.erase(Id);
    IdByPath.erase(Path);
    HelpInfo::ById.erase(Id);
    ChangeListenersById.erase(Id);
}

bool Component::IsChanged(bool include_descendents) const noexcept {
    return ChangedIds.contains(Id) || (include_descendents && IsDescendentChanged());
}

Component *Component::FindContainerByPath(const StorePath &search_path) {
    StorePath subpath = search_path;
    while (subpath != "/") {
        if (auto it = IdByPath.find(subpath); it != IdByPath.end() && ContainerIds.contains(it->second)) {
            return ById[it->second];
        }
        subpath = subpath.parent_path();
    }
    return nullptr;
}

// By default, a component is converted to JSON by visiting each of its leaf components (Fields) depth-first,
// and assigning the leaf's `json_pointer` to its JSON value.
json Component::ToJson() const {
    if (Children.empty()) return nullptr;

    std::stack<const Component *> to_visit;
    to_visit.push(this);

    json j;
    while (!to_visit.empty()) {
        const auto *current = to_visit.top();
        to_visit.pop();
        if (current->ChildCount() == 0) {
            auto leaf_json = current->ToJson();
            if (!leaf_json.is_null()) j[current->JsonPointer()] = std::move(leaf_json);
        } else {
            for (const auto *child : current->Children) {
                to_visit.push(child);
            }
        }
    }

    return j;
}

void Component::SetJson(json &&j) const {
    auto &&flattened = std::move(j).flatten(); // Don't inline this - it breaks `SetJson`.
    for (auto &&[key, value] : flattened.items()) {
        ByPath(std::move(key))->SetJson(std::move(value));
    }
}

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void Component::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) ImGui::SameLine();
    fg::HelpMarker(Help);
    if (!after) ImGui::SameLine();
}

const FlowGridStyle &Component::GetFlowGridStyle() const { return gStyle.FlowGrid; }

using namespace ImGui;

// Currently, `Draw` is not used for anything except wrapping around `Render`,
// but it's here in case we want to do something like monitoring or ID management in the future.
void Component::Draw() const {
    // ImGui widgets all push the provided label to the ID stack,
    // but info hovering isn't complete yet, and something like this might be needed...
    // PushID(ImGuiLabel.c_str());
    PushOverrideID(Id);
    Render();
    PopID();
}

void Menu::Render() const {
    if (Items.empty()) return;

    const bool is_menu_bar = Label.empty();
    if (IsMain ? BeginMainMenuBar() : (is_menu_bar ? BeginMenuBar() : BeginMenu(Label.c_str()))) {
        for (const auto &item : Items) {
            std::visit(
                Match{
                    [](const Menu &menu) { menu.Draw(); },
                    [](const MenuItemDrawable &drawable) { drawable.MenuItem(); },
                    [](const std::function<void()> &draw) { draw(); }
                },
                item
            );
        }
        if (IsMain) EndMainMenuBar();
        else if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}

void Component::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

ImGuiWindow *Component::FindWindow() const {
    return GetCurrentContext() ? FindWindowByName(ImGuiLabel.c_str()) : nullptr;
}

ImGuiWindow *Component::FindDockWindow() const {
    if (!GetCurrentContext()) return nullptr;
    auto *window = FindWindowByName(ImGuiLabel.c_str());
    return window && window->DockId ? window : (Parent ? Parent->FindDockWindow() : nullptr);
}

void Component::Dock(ID node_id) const {
    DockBuilderDockWindow(ImGuiLabel.c_str(), node_id);
}

bool Component::Focus() const {
    if (auto *window = FindWindow()) {
        FocusWindow(window);
        return true;
    }
    return false;
}

void Component::RenderTabs() const {
    if (BeginTabBar("")) {
        for (const auto *child : Children) {
            if (BeginTabItem(child->ImGuiLabel.c_str())) {
                child->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void Component::RenderTreeNodes(ImGuiTreeNodeFlags flags) const {
    for (const auto *child : Children) {
        if (TreeNodeEx(child->ImGuiLabel.c_str(), flags)) {
            child->Draw();
            TreePop();
        }
    }
}

void Component::OpenChanged() const { SetNextItemOpen(IsChanged(true)); }

void Component::ScrollToChanged() const {
    if (IsChanged(true) && IsItemVisible()) {
        ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
    }
}

bool Component::TreeNode(std::string_view label_view, bool highlight_label, const char *value, bool highlight_value, bool auto_select) const {
    if (auto_select) {
        OpenChanged();
        ScrollToChanged();
    }

    bool is_open = false;
    if (highlight_label) PushStyleColor(ImGuiCol_Text, GetFlowGridStyle().Colors[FlowGridCol_HighlightText]);
    if (value == nullptr) {
        const auto label = string(label_view);
        is_open = TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_None);
    } else if (!label_view.empty()) {
        const auto label = string(label_view);
        Text("%s: ", label.c_str()); // Render leaf label/value as raw text.
    }
    if (highlight_label) PopStyleColor();

    if (value != nullptr) {
        if (highlight_value) PushStyleColor(ImGuiCol_Text, GetFlowGridStyle().Colors[FlowGridCol_HighlightText]);
        SameLine();
        TextUnformatted(value);
        if (highlight_value) PopStyleColor();
    }
    return is_open;
}

void Component::TreePop() { ImGui::TreePop(); }
void Component::TextUnformatted(string_view text) { ImGui::TextUnformatted(string(text).c_str()); }

void Component::RenderValueTree(bool annotate, bool auto_select) const {
    if (Children.empty()) return TextUnformatted(Name.c_str());

    if (TreeNode(ImGuiLabel.empty() ? "Project" : ImGuiLabel, false, nullptr, false, auto_select)) {
        for (const auto *child : Children) {
            child->RenderValueTree(annotate, auto_select);
        }
        TreePop();
    }
}

void Component::FlashUpdateRecencyBackground(std::optional<StorePath> relative_path) const {
    if (const auto latest_update_time = LatestUpdateTime(Id, relative_path)) {
        const auto &style = GetFlowGridStyle();
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / style.FlashDurationSec;
        ImColor flash_color = style.Colors[FlowGridCol_Flash];
        flash_color.Value.w = std::max(0.f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }
}
