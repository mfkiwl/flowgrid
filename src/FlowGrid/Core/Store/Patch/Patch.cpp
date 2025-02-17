#include "Patch.h"

#include <algorithm>
#include <filesystem>

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    static constexpr auto AddOp = PatchOpType::Add;
    static constexpr auto RemoveOp = PatchOpType::Remove;
    static constexpr auto ReplaceOp = PatchOpType::Replace;

    PatchOps merged = a;
    for (const auto &[path, op] : b) {
        if (!merged.contains(path)) {
            merged[path] = op;
            continue;
        }

        const auto &old_op = merged.at(path);
        // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
        // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
        // For example, if the first patch removes a path, and the second one adds the same path,
        // we can't know from only looking at the pair whether the added value was the same as it was before the remove
        // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
        if (old_op.Op == AddOp) {
            if (op.Op == RemoveOp || ((op.Op == AddOp || op.Op == ReplaceOp) && old_op.Value == op.Value)) merged.erase(path); // Cancel out
            else merged[path] = {AddOp, op.Value, {}};
        } else if (old_op.Op == RemoveOp) {
            if (op.Op == AddOp || op.Op == ReplaceOp) {
                if (old_op.Value == op.Value) merged.erase(path); // Cancel out
                else merged[path] = {ReplaceOp, op.Value, old_op.Old};
            } else {
                merged[path] = {RemoveOp, {}, old_op.Old};
            }
        } else if (old_op.Op == ReplaceOp) {
            if (op.Op == AddOp || op.Op == ReplaceOp) merged[path] = {ReplaceOp, op.Value, old_op.Old};
            else merged[path] = {RemoveOp, {}, old_op.Old};
        }
    }

    return merged;
}

bool Patch::IsPrefixOfAnyPath(const StorePath &path) const noexcept {
    return std::ranges::any_of(GetPaths(), [&path](const StorePath &candidate_path) {
        const auto &[first_mismatched_path_it, _] = std::mismatch(path.begin(), path.end(), candidate_path.begin(), candidate_path.end());
        return first_mismatched_path_it == path.end();
    });
}
