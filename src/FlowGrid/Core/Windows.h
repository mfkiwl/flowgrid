#pragma once

#include "Core/Container/PrimitiveVector.h"
#include "WindowsAction.h"

struct Windows : Component, Actionable<Action::Windows::Any>, MenuItemDrawable {
    using Component::Component;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    bool IsWindow(ID id) const;
    void MenuItem() const override;

    Prop(PrimitiveVector<bool>, VisibleComponents);

    std::vector<ID> WindowComponentIds;

protected:
    void Render() const override;
};
