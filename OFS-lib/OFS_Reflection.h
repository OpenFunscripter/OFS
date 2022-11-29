#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <memory>
#include "glm/mat4x4.hpp"

#include "refl.hpp"

struct serializeEnum: refl::attr::usage::field, refl::attr::usage::function {
};


REFL_TYPE(glm::vec4)
REFL_FIELD(x)
REFL_FIELD(y)
REFL_FIELD(z)
REFL_FIELD(w)
REFL_END

REFL_TYPE(ImVec2)
REFL_FIELD(x)
REFL_FIELD(y)
REFL_END

REFL_TYPE(ImVec4)
REFL_FIELD(x)
REFL_FIELD(y)
REFL_FIELD(z)
REFL_FIELD(w)
REFL_END

REFL_TYPE(ImColor)
REFL_FIELD(Value)
REFL_END
