#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Flags,
    // todo toggle bit action instead of set
    DefineComponentAction(Set, "", int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
