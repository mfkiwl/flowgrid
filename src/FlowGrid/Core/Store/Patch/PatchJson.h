#pragma once

#include "Patch.h"

#include "Core/Json.h"
// #include "Core/Primitive/Primitive.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace nlohmann {
NLOHMANN_JSON_SERIALIZE_ENUM(
    PatchOp::Type,
    {
        {PatchOp::Type::Add, "add"},
        {PatchOp::Type::Remove, "remove"},
        {PatchOp::Type::Replace, "replace"},
    }
);
Json(PatchOp, Op, Value, Old);
Json(Patch, Ops, BasePath);
} // namespace nlohmann
