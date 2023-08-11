#pragma once

#include <ranges>
#include <unordered_map>

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"

// todo use `IsPrimitive` concept instead of holding `Primitive` values.
// Need to think about how.
struct PatchOp {
    enum Type {
        Add,
        Remove,
        Replace,
    };

    Type Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};

std::string to_string(PatchOp::Type);

using PatchOps = std::unordered_map<StorePath, PatchOp, PathHash>;
PatchOps Merge(const PatchOps &a, const PatchOps &b);

struct Patch {
    PatchOps Ops;
    StorePath BasePath{RootPath};

    // Returns a view.
    inline auto GetPaths() const noexcept {
        return Ops | std::views::keys | std::views::transform([this](const auto &partial_path) { return BasePath / partial_path; });
    }

    bool Empty() const noexcept { return Ops.empty(); }
    bool IsPrefixOfAnyPath(const StorePath &) const noexcept;
};
