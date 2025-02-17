#pragma once

#include <tuple>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include "immer/vector.hpp"

#include "Core/Action/Actionable.h"
#include "Helper/Path.h"
#include "IdPairs.h"
#include "Patch/Patch.h"
#include "StoreAction.h"

// Utility to transform a tuple of types into a tuple of types wrapped by a wrapper type.
template<template<typename> class WrapperType, typename TypesTuple> struct WrapTypes;
template<template<typename> class WrapperType, typename... Types> struct WrapTypes<WrapperType, std::tuple<Types...>> {
    using type = std::tuple<WrapperType<Types>...>;
};

struct Store : Actionable<Action::Store::Any> {
    template<typename T> using Map = immer::map<StorePath, T, PathHash>;
    template<typename T> using TransientMap = immer::map_transient<StorePath, T, PathHash>;

    using ValueTypes = std::tuple<bool, u32, s32, float, std::string, IdPairs, immer::set<u32>>;
    using StoreMaps = typename WrapTypes<Map, ValueTypes>::type;
    using TransientStoreMaps = typename WrapTypes<TransientMap, ValueTypes>::type;

    // The store starts in transient mode.
    Store() : TransientMaps(std::make_unique<TransientStoreMaps>()) {}
    Store(const Store &other) noexcept { Set(other); }
    Store(Store &&other) noexcept { Set(std::move(other)); }
    ~Store() = default;

    bool CanApply(const ActionType &) const override { return true; }
    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const Action::Store::ApplyPatch &a) { ApplyPatch(a.patch); },
            },
            action
        );
    }

    template<typename ValueType> const Map<ValueType> &GetMap() const { return std::get<Map<ValueType>>(Maps); }

    template<typename ValueType> const ValueType &Get(const StorePath &path) const { return GetTransientMap<ValueType>().at(path); }
    template<typename ValueType> u32 CountAt(const StorePath &path) const { return GetTransientMap<ValueType>().count(path); }

    template<typename ValueType> void Set(const StorePath &path, const ValueType &value) const { GetTransientMap<ValueType>().set(path, value); }
    template<typename ValueType> void Erase(const StorePath &path) const { GetTransientMap<ValueType>().erase(path); }
    void ErasePrimitive(const StorePath &path) const {
        if (Contains<bool>(path)) Erase<bool>(path);
        else if (Contains<u32>(path)) Erase<u32>(path);
        else if (Contains<s32>(path)) Erase<s32>(path);
        else if (Contains<float>(path)) Erase<float>(path);
        else if (Contains<std::string>(path)) Erase<std::string>(path);
    }

    template<typename ValueType> bool Contains(const StorePath &path) const { return CountAt<ValueType>(path) > 0; }

    bool ContainsPrimitive(const StorePath &path) const {
        return Contains<bool>(path) || Contains<u32>(path) || Contains<s32>(path) || Contains<float>(path) || Contains<std::string>(path);
    }

    bool Contains(const StorePath &path) const {
        // xxx this is the only place in the store where we use knowledge about vector paths.
        return ContainsPrimitive(path) || ContainsPrimitive(path / "0") || Contains<IdPairs>(path);
    }

    // Overwrite the store with the provided store and return the resulting patch.
    Patch CheckedSet(const Store &store) {
        const auto patch = CreatePatch(store);
        Set(store);
        return patch;
    }

    // Overwrite the persistent store with all changes since the last commit.
    void Commit() { Set(*this); }
    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit() { return CheckedSet(Store{*this}); }

    // Create a patch comparing the provided stores.
    Patch CreatePatch(const Store &before, const Store &after, const StorePath &base_path = RootPath) const;

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const Store &store, const StorePath &base_path = RootPath) const { return CreatePatch(*this, store, base_path); }
    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(const StorePath &base_path = RootPath) {
        const auto patch = CreatePatch(*this, Store{*this}, base_path);
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
        return patch;
    }

private:
    StoreMaps Maps;
    std::unique_ptr<TransientStoreMaps> TransientMaps; // If this is non-null, the store is in transient mode.

    StoreMaps Get() const { return TransientMaps ? Persistent() : Maps; }
    void Set(Store &&other) noexcept {
        Maps = other.Get();
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }
    void Set(const Store &other) noexcept {
        Maps = other.Get();
        TransientMaps = std::make_unique<TransientStoreMaps>(Transient());
    }

    StoreMaps Persistent() const;
    TransientStoreMaps Transient() const;

    template<typename ValueType> TransientMap<ValueType> &GetTransientMap() const { return std::get<TransientMap<ValueType>>(*TransientMaps); }

    void ApplyPatch(const Patch &patch) const {
        for (const auto &[partial_path, op] : patch.Ops) {
            const auto path = patch.BasePath / partial_path;
            std::visit([this, &path, &op](auto &&v) {
                if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) Set(path, std::move(v));
                else if (op.Op == PatchOpType::Remove) ErasePrimitive(path);
            },
                       *op.Value);
        }
    }
};
