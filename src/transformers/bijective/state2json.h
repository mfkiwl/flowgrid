#pragma once

#include "nlohmann/json.hpp"
#include "../../state.h"

using json = nlohmann::json;

json state2json(const State &);
