#pragma once
#include "OFS_Reflection.h"
#include "nlohmann/json.hpp"

#include "OFS_Util.h"

#include <array>

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
		
		template<typename T>
		inline archiver& operator<<(reflect_member<std::vector<T>>& pair);

		template<typename T, size_t arraySize>
		inline archiver& operator<<(reflect_member<std::array<T, arraySize>>& pair);
	};


	class unpacker {
	public:
		nlohmann::json* ctx;
		unpacker(nlohmann::json* node) : ctx(node) {}

		template<typename T>
		inline unpacker& operator<<(reflect_member<T>& pair);

		template<typename T>
		inline unpacker& operator<<(reflect_member<std::vector<T>>& pair);

		template<typename T, size_t arraySize>
		inline unpacker& operator<<(reflect_member<std::array<T, arraySize>>& pair);
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

	template<typename T>
	inline archiver& archiver::operator<<(reflect_member<std::vector<T>>& pair) {
		auto* node = &(*ctx)[pair.name];
		*node = nlohmann::json::array();
		for (auto& item : *pair.value) {
			nlohmann::json itemJson;
			if constexpr (BlackMagic::has_reflect<T, archiver>::value) {
				archiver ar(&itemJson);
				item.template reflect<archiver>(ar);
				(*node).push_back(itemJson);
			}
			else if constexpr (BlackMagic::has_reflect_function<T, archiver>::value) {
				archiver ar(&itemJson);
				reflect_function<T, archiver>().reflect(item, ar);
				(*node).push_back(itemJson);
			}
			else {
				(*node) = *pair.value;
				break;
			}
		}
		return *this;
	}


	template<typename T, size_t arraySize>
	inline archiver& archiver::operator<<(reflect_member<std::array<T, arraySize>>& pair) {
		auto* node = &(*ctx)[pair.name];
		*node = nlohmann::json::array();
		for (auto& item : *pair.value) {
			nlohmann::json itemJson;
			if constexpr (BlackMagic::has_reflect<T, archiver>::value) {
				archiver ar(&itemJson);
				item.template reflect<archiver>(ar);
				(*node).push_back(itemJson);
			}
			else if constexpr (BlackMagic::has_reflect_function<T, archiver>::value) {
				archiver ar(&itemJson);
				reflect_function<T, archiver>().reflect(item, ar);
				(*node).push_back(itemJson);
			}
			else {
				for (auto&& item : *pair.value)
				{
					(*node).push_back(item);
				}
				break;
			}
		}
		return *this;
	}


	// IMPLEMENTATION unpacker
	//==================================================================
	template<typename T>
	inline unpacker& unpacker::operator<<(reflect_member<T>& pair)
	{
		auto* node = &(*ctx)[pair.name];
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

	template<typename T>
	inline unpacker& unpacker::operator<<(reflect_member<std::vector<T>>& pair) {
		auto* node = &(*ctx)[pair.name];
		if (node->is_array()) {
			for (auto& item : *node) {
				if constexpr (BlackMagic::has_reflect<T, unpacker>::value) {
					unpacker des(&item);
					T unpacked;
					unpacked.template reflect<unpacker>(des);
					pair.value->push_back(unpacked);
				}
				else if constexpr (BlackMagic::has_reflect_function<T, unpacker>::value) {
					unpacker des(&item);
					T unpacked;
					reflect_function<T, unpacker>().reflect(unpacked, des);
					pair.value->push_back(unpacked);
				}
				else {
					if (node->is_null()) {
						LOGF_WARN("Failed to reflect \"%s\"", pair.name);
					}
					else {
						pair.value->emplace_back(std::move(item.template get<T>()));
					}
				}
			}
		}
		return *this;
	}

	template<typename T, size_t arraySize>
	inline unpacker& unpacker::operator<<(reflect_member<std::array<T, arraySize>>& pair) {
		auto* node = &(*ctx)[pair.name];
		if (node->is_array()) {
			int32_t index = 0;
			for (auto& item : *node) {
				if constexpr (BlackMagic::has_reflect<T, unpacker>::value) {
					unpacker des(&item);
					(*pair.value)[index].template reflect<unpacker>(des);
				}
				else if constexpr (BlackMagic::has_reflect_function<T, unpacker>::value) {
					unpacker des(&item);
					reflect_function<T, unpacker>().relfect((*pair.value)[index], des);
				}
				else {
					if (node->is_null()) {
						LOGF_WARN("Failed to reflect \"%s\"", pair.name);
					}
					else {
						(*pair.value)[index] = item.template get<T>();
					}
				}
				index++;
			}
		}
		return *this;
	}
}