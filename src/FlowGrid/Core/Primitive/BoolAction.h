#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Bool,
    DefineComponentAction(Toggle, "");
    Json(Toggle, path);

    using Any = ActionVariant<Toggle>;
);
