#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <memory>
#include "glm/mat4x4.hpp"

#include "refl.hpp"

struct serializeEnum: refl::attr::usage::field, refl::attr::usage::function {
};

template<typename Wrapped>
struct Serializable { /* Can be specialized like below */
};

template<>
struct Serializable<glm::mat4> {
    /* 
        glm::mat4 can't be serialized by default because the data is private and only accessible 
        via the operator[] function.
        This wrapper adds const & mutable getter functions to allow serialization.
    */
    glm::mat4 Value;
    inline Serializable() noexcept {}
    inline Serializable(glm::mat4& m) : Value(m) {}

    auto& operator=(const glm::mat4& m) noexcept
    {
        Value = m;
        return *this;
    }

    inline operator glm::mat4() const { return Value; }

    inline glm::vec4& Row0() noexcept { return Value[0]; }
    inline glm::vec4& Row1() noexcept { return Value[1]; }
    inline glm::vec4& Row2() noexcept { return Value[2]; }
    inline glm::vec4& Row3() noexcept { return Value[3]; }
    inline const glm::vec4& Row0() const noexcept { return Value[0]; }
    inline const glm::vec4& Row1() const noexcept { return Value[1]; }
    inline const glm::vec4& Row2() const noexcept { return Value[2]; }
    inline const glm::vec4& Row3() const noexcept { return Value[3]; }
};

REFL_TYPE(glm::vec4)
REFL_FIELD(x)
REFL_FIELD(y)
REFL_FIELD(z)
REFL_FIELD(w)
REFL_END

REFL_TYPE(Serializable<glm::mat4>)
REFL_FUNC(Row0, property{ "Row0" })
REFL_FUNC(Row1, property{ "Row1" })
REFL_FUNC(Row2, property{ "Row2" })
REFL_FUNC(Row3, property{ "Row3" })
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
