#pragma once

#include "AdjacencyListAction.h"
#include "NavigableAction.h"
#include "PrimitiveSetAction.h"
#include "PrimitiveVector2DAction.h"
#include "PrimitiveVectorAction.h"
#include "Vec2Action.h"

namespace Action {
namespace Container {
using Any = Combine<
    AdjacencyList::Any, Navigable<u32>::Any, Vec2::Any,
    PrimitiveSet<u32>::Any,
    PrimitiveVector<bool>::Any, PrimitiveVector<int>::Any, PrimitiveVector<u32>::Any, PrimitiveVector<float>::Any, PrimitiveVector<std::string>::Any,
    PrimitiveVector2D<bool>::Any, PrimitiveVector2D<int>::Any, PrimitiveVector2D<u32>::Any, PrimitiveVector2D<float>::Any>;
} // namespace Container
} // namespace Action
