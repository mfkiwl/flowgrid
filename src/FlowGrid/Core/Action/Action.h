#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "Helper/Path.h"
#include "Helper/Variant.h"

using json = nlohmann::json;

/**
An action is an immutable and complete representation of a user interaction event affecting application state.
Each action stores all the information needed to apply it to a `Store` instance.
An `ActionMoment` is a combination of an action and the `TimePoint` when action was queued.

Actions are grouped into `ActionVariant`s, which wrap around `std::variant`.
Thus, `Action::Any` has enough bytes to hold the application's largest action type.
- For actions holding very large structured data, using a JSON string is a good approach to keep the size low
  (at the expense of losing type safety, incurring (de-)serialization costs, and storing the string contents in heap memory).
- Note that adding static members does not increase the size of the variant(s) it belongs to.
- Metrics->FlowGrid->'Action variant size' shows the byte size of `Action::Any`.
*/
namespace Action {
struct Metadata {
    /**
    `meta_str` is of the format: "~{menu label}" (order-independent, prefixes required).
    Add `!` to the beginning of the string to indicate that the action should not be saved to the undo stack
    (or added to the gesture history, or saved in a `.fga` (FlowGridAction) project).
    This is used for actions with only non-state-updating side effects, like saving a file.
    */
    Metadata(std::string_view path_leaf, std::string_view meta_str = "");

    const std::string PathLeaf; // E.g. "Set"
    const std::string Name; // Human-readable name. By default, derived as `PascalToSentenceCase(PathLeaf)`.
    const std::string MenuLabel; // Defaults to `Name`.

    // todo
    // const ID Id;
    // const string Help, ImGuiLabel;

private:
    struct Parsed {
        const std::string MenuLabel;
    };
    Parsed Parse(std::string_view meta_str);

    Metadata(std::string_view path_leaf, Parsed parsed);
};

template<typename T>
concept IsAction = requires(T t) {
    { T::_Meta } -> std::same_as<const Metadata &>;
    { T::IsSaved } -> std::same_as<const bool &>;
};

template<IsAction T> struct IsSaved {
    static constexpr bool value = T::IsSaved;
};
template<IsAction T> struct IsNotSaved {
    static constexpr bool value = !T::IsSaved;
};

template<IsAction... T> struct ActionVariant : std::variant<T...> {
    using variant_t = std::variant<T...>; // Alias for the base variant type.
    using variant_t::variant; // Inherit the base variant's ctors.

    // Note: these maps are declared to be instantiated for each `ActionVariant` type,
    // but the compiler only instantiates them for the types with references to the map.
    template<size_t I = 0> static auto CreatePathToIndex() {
        if constexpr (I < std::variant_size_v<variant_t>) {
            using MemberType = std::variant_alternative_t<I, variant_t>;
            auto map = CreatePathToIndex<I + 1>();
            map[MemberType::GetPath()] = I;
            return map;
        }
        return std::unordered_map<fs::path, size_t, PathHash>{};
    }

    size_t GetIndex() const { return this->index(); }

    fs::path GetPath() const {
        return Call([](auto &a) { return a.GetPath(); });
    }
    fs::path GetComponentPath() const {
        return Call([](auto &a) { return a.GetComponentPath(); });
    }
    std::string GetMenuLabel() const {
        return Call([](auto &a) { return a.GetMenuLabel(); });
    }

    /**
     Provided actions are assumed to be chronologically consecutive.

     Cases:
     * `b` can be merged into `a`: return the merged action
     * `b` cancels out `a` (e.g. two consecutive boolean toggles on the same value): return `true`
     * `b` cannot be merged into `a`: return `false`

     Only handling cases where merges can be determined from two consecutive actions.
     One could imagine cases where an idempotent cycle could be determined only from > 2 actions.
     For example, incrementing modulo N would require N consecutive increments to determine that they could all be cancelled out.
    */
    using MergeResult = std::variant<ActionVariant, bool>;
    MergeResult Merge(const ActionVariant &other) const {
        if (GetIndex() != other.GetIndex()) return false;
        return Call([&other](auto &a) {
            const auto &result = a.Merge(std::get<std::decay_t<decltype(a)>>(other));
            if (std::holds_alternative<bool>(result)) return MergeResult(std::get<bool>(result));
            return MergeResult(ActionVariant{std::get<std::decay_t<decltype(a)>>(result)});
        });
    }

    template<size_t I = 0> static ActionVariant Create(size_t index) {
        if constexpr (I >= std::variant_size_v<variant_t>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
        else return index == 0 ? ActionVariant{std::in_place_index<I>} : Create<I + 1>(index - 1);
    }

    // Construct a variant from its index and JSON representation.
    // Adapted for JSON from the default-ctor approach here: https://stackoverflow.com/a/60567091/780425
    template<size_t I = 0> static ActionVariant Create(size_t index, const json &j) {
        if constexpr (I >= std::variant_size_v<variant_t>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
        else return index == 0 ? j.get<std::variant_alternative_t<I, variant_t>>() : Create<I + 1>(index - 1, j);
    }

    // Serialize actions as two-element arrays, `[Path, Data]`.
    // Value element can possibly be null.
    // Assumes all actions define json converters.
    void to_json(json &j) const {
        Call([&j](auto &a) { j = {a.GetPath(), a}; });
    }

    inline static void from_json(const json &j, ActionVariant &value) {
        static auto PathToIndex = CreatePathToIndex();
        const auto path = j[0].get<fs::path>();
        value = Create(PathToIndex[path], j[1]);
    }

private:
    // Call a function on the variant's active member type.
    template<typename Callable> auto Call(Callable func) const {
        return std::visit([&func](const auto &action) { return func(action); }, *this);
    }
};

// Flatten two or more `ActionVariant`s together into one variant.
// E.g. `using FooAction = Action::Combine<ActionVariant1, ActionVariant2, ActionVariant3>`
template<typename... Vars> struct CombineImpl;
template<typename Var> struct CombineImpl<Var> {
    using type = Var;
};
template<typename... Ts1, typename... Ts2, typename... Vars>
struct CombineImpl<ActionVariant<Ts1...>, ActionVariant<Ts2...>, Vars...> {
    using type = CombineImpl<ActionVariant<Ts1..., Ts2...>, Vars...>::type;
};
template<typename... Vars> using Combine = typename CombineImpl<Vars...>::type;

// Append a (single, non-variant) action type to an `ActionVariant`.
// E.g. `using BarAction = Action::Append<FooAction, Bar>`
template<typename Var, typename T> struct AppendImpl;
template<typename... Ts, typename T> struct AppendImpl<ActionVariant<Ts...>, T> {
    using type = ActionVariant<Ts..., T>;
};
template<typename Var, typename T> using Append = typename AppendImpl<Var, T>::type;

// Filter an `ActionVariant` by a predicate.
// E.g. `using Saved = Action::Filter<Action::IsSaved, Any>;`
template<template<typename> class Predicate, typename Var> struct FilterImpl;
template<template<typename> class Predicate, typename... Types> struct FilterImpl<Predicate, Action::ActionVariant<Types...>> {
    template<typename Type>
    using ConditionalAdd = std::conditional_t<Predicate<Type>::value, Action::ActionVariant<Type>, Action::ActionVariant<>>;
    using type = Action::Combine<ConditionalAdd<Types>...>;
};
template<template<typename> class Predicate, typename... Types> using Filter = typename FilterImpl<Predicate, Types...>::type;
} // namespace Action
