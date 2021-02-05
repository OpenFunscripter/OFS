#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <memory>

#include "glm/mat4x4.hpp"

template<typename T>
struct reflect_member {
	reflect_member(const char* name, T* ptr) {
		this->value = ptr;
		this->name = name;
	}
	const char* name;
	T* value;
};

#define OFS_REFLECT(member, archive) { auto pair = reflect_member(#member, std::addressof(member)); archive << pair; }
#define OFS_REFLECT_PTR(member, archive) { auto pair = reflect_member(#member, member); archive << pair; }
#define OFS_REFLECT_PTR_NAMED(name, member, archive) { auto pair = reflect_member(name, member); archive << pair; }
#define OFS_REFLECT_NAMED(name, value, archive) { auto pair = reflect_member(name, std::addressof(value)); archive << pair; }

template<typename T, typename Archive>
struct reflect_function {
	/*
	// define this in a specialization
	// for third party types
	void reflect(T& obj, Archive& ar) {
	}
	*/
};


// specializations
template<typename Archive>
struct reflect_function<ImVec2, Archive> 
{
	void reflect(ImVec2& obj, Archive& ar) {
		OFS_REFLECT_NAMED("x", obj.x, ar);
		OFS_REFLECT_NAMED("y", obj.y, ar);
	}
};

template<typename Archive>
struct reflect_function<ImColor, Archive>
{
	void reflect(ImColor& obj, Archive& ar) {
		OFS_REFLECT_NAMED("x", obj.Value.x, ar);
		OFS_REFLECT_NAMED("y", obj.Value.y, ar);
		OFS_REFLECT_NAMED("z", obj.Value.z, ar);
		OFS_REFLECT_NAMED("w", obj.Value.w, ar);
	}
};

template<typename Archive>
struct reflect_function<ImRect, Archive>
{
	void reflect(ImRect& obj, Archive& ar) {
		OFS_REFLECT_NAMED("Min", obj.Min, ar);
		OFS_REFLECT_NAMED("Max", obj.Max, ar);
	}
};

template<typename Archive>
struct reflect_function<glm::mat4, Archive>
{
	void reflect(glm::mat4& obj, Archive& ar)
	{
		OFS_REFLECT(obj[0][0], ar);
		OFS_REFLECT(obj[0][1], ar);
		OFS_REFLECT(obj[0][2], ar);
		OFS_REFLECT(obj[0][3], ar);
		OFS_REFLECT(obj[1][0], ar);
		OFS_REFLECT(obj[1][1], ar);
		OFS_REFLECT(obj[1][2], ar);
		OFS_REFLECT(obj[1][3], ar);
		OFS_REFLECT(obj[2][0], ar);
		OFS_REFLECT(obj[2][1], ar);
		OFS_REFLECT(obj[2][2], ar);
		OFS_REFLECT(obj[2][3], ar);
		OFS_REFLECT(obj[3][0], ar);
		OFS_REFLECT(obj[3][1], ar);
		OFS_REFLECT(obj[3][2], ar);
		OFS_REFLECT(obj[3][3], ar);
	}
};