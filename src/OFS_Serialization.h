#pragma once

#include "OFS_Reflection.h"
#include "nlohmann/json.hpp"

namespace BlackMagic {

	template <typename T, typename A>
	struct has_reflect {
		template <typename Obj, typename ar>
		static constexpr decltype(std::declval<Obj>().template reflect<ar>(std::declval<ar&>()), bool()) test_get(int) { return true; }

		template <typename Obj, typename ar>
		static constexpr bool test_get(...) { return false; }

		static constexpr bool value = test_get<T, A>(int());
	};

	template <typename T, typename A>
	struct has_reflect_function {
		template <typename Obj, typename ar>
		static constexpr decltype(std::declval<reflect_function<Obj, ar>>().reflect(std::declval<Obj&>(), std::declval<ar&>()), bool()) test_get(int) { return true; }

		template <typename Obj, typename ar>
		static constexpr bool test_get(...) { return false; }

		static constexpr bool value = test_get<T, A>(int());
	};
}

namespace OFS
{

	class archiver {
	public:
		nlohmann::json* ctx;
		archiver(nlohmann::json* node) : ctx(node) {}
		template<typename T>
		inline archiver& operator<<(reflect_member<T>& pair);
	};


	class unpacker {
	public:
		nlohmann::json* ctx;
		unpacker(nlohmann::json* node) : ctx(node) {}
		template<typename T>
		inline unpacker& operator<<(reflect_member<T>& pair);
	};


	class serializer {
	public:
		template<typename T, typename ar = OFS::unpacker>
		inline static void load(T* obj, nlohmann::json* json) {
			ar deserializer(json);
			obj->reflect(deserializer);
		}

		template<typename T, typename ar = OFS::archiver>
		inline static void save(T* obj, nlohmann::json* json) {
			ar archive(json);
			obj->reflect(archive);
		}
	};


	// IMPLEMENTATION archiver
	//==================================================================
	template<typename T>
	inline archiver& archiver::operator<<(reflect_member<T>& pair)
	{
		auto* node = &(*ctx)[pair.name];
		if constexpr (BlackMagic::has_reflect<T, archiver>::value) {
			archiver ar(node);
			pair.value->template reflect<archiver>(ar);
			(*ctx)[pair.name] = *ar.ctx;
		}
		else if constexpr (BlackMagic::has_reflect_function<T, archiver>::value) {
			archiver ar(node);
			reflect_function<T, archiver>().reflect(*pair.value, ar);
			(*ctx)[pair.name] = *ar.ctx;
		}
		else {
			(*ctx)[pair.name] = *pair.value;
		}
		return *this;
	}

	// IMPLEMENTATION unpacker
	//==================================================================
	template<typename T>
	inline unpacker& unpacker::operator<<(reflect_member<T>& pair)
	{
		auto* node = &(*ctx)[pair.name];
		if (node == nullptr) return *this;
		if constexpr (BlackMagic::has_reflect<T, unpacker>::value) {
			unpacker des(node);
			pair.value->template reflect<unpacker>(des);
		}
		else if constexpr (BlackMagic::has_reflect_function<T, unpacker>::value) {
			unpacker des(node);
			reflect_function<T, unpacker>().reflect(*pair.value, des);
		}
		else {
			if (node->is_null()) {
				LOGF_WARN("Failed to reflect \"%s\"", pair.name);
			}
			else {
				*pair.value = node->template get<T>();
			}
		}
		return *this;
	}
}