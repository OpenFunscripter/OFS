#pragma once

#include "imgui.h"

#include <memory>

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
#define OFS_REFLECT_NAMED(name, value, archive) { auto pair = reflect_member(#name, std::addressof(value)); archive << pair; }

template<typename T, typename Archive>
struct reflect_function {
	/*
	// define this in a specialization
	// for third party types
	void reflect(T& obj, Archive& ar) {
	}
	*/
};


template<typename Archive>
struct reflect_function<ImVec2, Archive> 
{
	void reflect(ImVec2& obj, Archive& ar) {
		OFS_REFLECT_NAMED(x, obj.x, ar);
		OFS_REFLECT_NAMED(y, obj.y, ar);
	}
};

template<typename Archive>
struct reflect_function<ImColor, Archive>
{
	void reflect(ImColor& obj, Archive& ar) {
		OFS_REFLECT_NAMED(x, obj.Value.x, ar);
		OFS_REFLECT_NAMED(y, obj.Value.y, ar);
		OFS_REFLECT_NAMED(z, obj.Value.z, ar);
		OFS_REFLECT_NAMED(w, obj.Value.w, ar);
	}
};
