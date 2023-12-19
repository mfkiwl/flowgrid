#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    TextBuffer,
    DefineComponentAction(Set, "", std::string value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
