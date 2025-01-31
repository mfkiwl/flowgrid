#pragma once

#include "Action.h"
#include "Core/Json.h"
#include "Core/Primitive/Scalar.h" // Not actually used in this file, but included as a convenience for action definitions.

// Component actions hold a `path` to the component they act on.
#define ComponentActionJson(ActionType, ...) Json(ActionType, path __VA_OPT__(, ) __VA_ARGS__);

template<class...> constexpr bool always_false_v = false;

/* Macros for defining actions */
#define MergeType_NoMerge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &) const { return false; }
#define MergeType_Merge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { return other; }
#define MergeType_CustomMerge(ActionType) std::variant<ActionType, bool> Merge(const ActionType &) const;
#define MergeType_SamePathMerge(ActionType)                                      \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { \
        if (this->path == other.path) return other;                              \
        return false;                                                            \
    }

/**
* Pass `is_savable = 1` to declare the action as savable (undoable, gesture history, saved in `.fga` projects).
* Use `action.q()` to queue the action. _Note: `q` methods for all action types are defined in `Project.cpp`._
* Merge types:
  - `NoMerge`: Cannot be merged with any other action.
  - `Merge`: Can be merged with any other action of the same type.
  - `CustomMerge`: Override the action type's `Merge` function with a custom implementation.
*/
#define DefineActionInternal(ActionType, is_savable, merge_type, meta_str, ...) \
    struct ActionType {                                                         \
        inline static const Metadata _Meta{#ActionType, meta_str};              \
        static constexpr bool IsSaved = is_savable;                             \
        void q() const;                                                         \
        static void MenuItem();                                                 \
        static fs::path GetPath() { return _TypePath / _Meta.PathLeaf; }        \
        static const std::string &GetName() { return _Meta.Name; }              \
        static const std::string &GetMenuLabel() { return _Meta.MenuLabel; }    \
        MergeType_##merge_type(ActionType);                                     \
        __VA_ARGS__;                                                            \
    };

#define DefineAction(ActionType, merge_type, meta_str, ...) \
    DefineActionInternal(ActionType, 1, merge_type, meta_str, __VA_ARGS__)

#define DefineUnmergableAction(ActionType, ...) \
    DefineActionInternal(ActionType, 1, NoMerge, "", __VA_ARGS__)

#define DefineUnsavedAction(ActionType, merge_type, meta_str, ...) \
    DefineActionInternal(ActionType, 0, merge_type, meta_str, __VA_ARGS__)

#define DefineComponentAction(ActionType, meta_str, ...)               \
    DefineActionInternal(                                              \
        ActionType, 1, SamePathMerge, meta_str,                        \
        fs::path path;                                                 \
        fs::path GetComponentPath() const { return path; } __VA_ARGS__ \
    )

#define DefineUnsavedComponentAction(ActionType, merge_type, meta_str, ...) \
    DefineActionInternal(                                                   \
        ActionType, 0, merge_type, meta_str,                                \
        fs::path path;                                                      \
        fs::path GetComponentPath() const { return path; } __VA_ARGS__      \
    )

#define DefineUnmergableComponentAction(ActionType, ...)               \
    DefineActionInternal(                                              \
        ActionType, 1, NoMerge, "",                                    \
        fs::path path;                                                 \
        fs::path GetComponentPath() const { return path; } __VA_ARGS__ \
    )

#define DefineActionType(TypePath, ...)                \
    namespace Action {                                 \
    namespace TypePath {                               \
    inline static const fs::path _TypePath{#TypePath}; \
    __VA_ARGS__;                                       \
    }                                                  \
    }

#define DefineNestedActionType(ParentType, InnerType, ...)                      \
    namespace Action {                                                          \
    namespace ParentType {                                                      \
    namespace InnerType {                                                       \
    inline static const fs::path _TypePath{fs::path{#ParentType} / #InnerType}; \
    __VA_ARGS__;                                                                \
    }                                                                           \
    }                                                                           \
    }

#define DefineTemplatedActionType(ParentType, InnerType, TemplateType, ...)         \
    template<> struct ParentType<TemplateType> {                                    \
        inline static const fs::path _TypePath{fs::path{#ParentType} / #InnerType}; \
        __VA_ARGS__;                                                                \
    }
