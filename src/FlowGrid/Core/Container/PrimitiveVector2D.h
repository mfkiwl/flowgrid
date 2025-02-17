#pragma once

#include "Container.h"
#include "Core/Action/Actionable.h"
#include "PrimitiveVector2DAction.h"

// PrimitiveVector of vectors. Inner vectors may have different sizes.
template<typename T> struct PrimitiveVector2D : Container, Actionable<typename Action::PrimitiveVector2D<T>::Any> {
    using Container::Container;

    using ActionT = typename Action::PrimitiveVector2D<T>;
    using typename Actionable<typename ActionT::Any>::ActionType; // See note in `PrimitiveVector.h`.

    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const ActionT::Set &a) { Set(a.value); },
            },
            action
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    void SetJson(json &&) const override;
    json ToJson() const override;

    void Refresh() override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    T operator()(u32 i, u32 j) const { return Value[i][j]; }

    StorePath PathAt(const u32 i, const u32 j) const { return Path / std::to_string(i) / std::to_string(j); }
    u32 Size() const { return Value.size(); }; // Number of outer vectors
    u32 Size(u32 i) const { return Value[i].size(); }; // Size of inner vector at index `i`

    void Set(const std::vector<std::vector<T>> &) const;
    void Set(u32 i, u32 j, const T &) const;
    void Resize(u32 size) const;
    void Resize(u32 i, u32 size) const;
    void Erase() const override;

private:
    std::vector<std::vector<T>> Value;
};
