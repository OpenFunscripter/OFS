#pragma once
#include "OFS_Reflection.h"
#include "nlohmann/json.hpp"

#include "OFS_Util.h"

#include <array>
#include <iostream>

namespace OFS
{
	template<typename T>
	struct is_json_compatible : std::false_type {};
	template<>
	struct is_json_compatible<std::string> : std::true_type {};
	template<>
	struct is_json_compatible<float> : std::true_type {};
	template<>
	struct is_json_compatible<size_t> : std::true_type {};
	template<>
	struct is_json_compatible<int64_t> : std::true_type {};
	template<>
	struct is_json_compatible<int32_t> : std::true_type {};
	template<>
	struct is_json_compatible<uint16_t> : std::true_type {};
	template<>
	struct is_json_compatible<bool> : std::true_type {};
	template<>
	struct is_json_compatible<char> : std::true_type {};
	
	class Serializer {
	private:
		template<typename FieldDescriptor, typename ObjectType>
		inline static auto& GetFieldRef(FieldDescriptor desc, ObjectType& obj) noexcept
		{
			if constexpr (refl::descriptor::is_field(desc)) {
				if constexpr(refl::descriptor::is_static(desc)) {
					return desc();
				}
				else {
					return desc(obj);
				}
			}
			else if constexpr(refl::descriptor::is_property(desc)) {
				auto& mutableGetterResult = desc(obj);
				return mutableGetterResult;
			}
		}

		template<typename FieldDescriptor, typename ObjectType>
		inline static auto& GetConstFieldRef(FieldDescriptor desc, const ObjectType& obj) noexcept
		{
			if constexpr (refl::descriptor::is_field(desc)) {
				if constexpr(refl::descriptor::is_static(desc)) {
					return desc();
				}
				else {
					return desc(obj);
				}
			}
			else if constexpr(refl::descriptor::is_property(desc)) {
				auto& getterResult = desc(obj);
				return getterResult;
			}
		}

		template<typename ObjectType>
		inline static bool deserializeObject(ObjectType& objectRef, const nlohmann::json& objectJson) noexcept
		{
			static_assert(!OFS::is_json_compatible<ObjectType>::value);

			bool successful = true;
			for_each(refl::reflect(objectRef).members, [&](auto member) noexcept
			{
				auto& memberRef = GetFieldRef(member, objectRef);
				using MemberType = std::remove_reference<decltype(memberRef)>::type;

				// Check if the json object contains the key,
				// if not a warning is logged but the deserialization continues.
				if(objectJson.contains(get_display_name(member))) {
					auto& currentJson = objectJson[get_display_name(member)];
					bool succ = OFS::Serializer::Deserialize(memberRef, currentJson);
					if(!succ) successful = false;
				}
				else {
					LOGF_WARN("The field \"%s\" was not found.", get_display_name(member));
				}
			});

			return successful;
		}

		template<typename ItemType>
		inline static bool deserializeContainerItems(std::vector<ItemType>& obj, const nlohmann::json& jsonArray) noexcept
		{
			for(auto& jsonItem : jsonArray) {
				auto& item = obj.emplace_back();
				bool succ = OFS::Serializer::Deserialize(item, jsonItem);
				if(!succ) return false;
			}
			return true;
		}

		template<typename ItemType, size_t Size>
		inline static bool deserializeContainerItems(std::array<ItemType, Size>& obj, const nlohmann::json& jsonArray) noexcept
		{
			size_t idx = 0;
			for(auto& jsonItem : jsonArray) {
				auto& item = obj[idx++];
				bool succ = OFS::Serializer::Deserialize(item, jsonItem);
				if(!succ) return false;
			}
			return true;
		}
	public:
		template<typename T>
		inline static bool Deserialize(T& obj, const nlohmann::json& json) noexcept
		{
			static_assert(!std::is_const_v<T>);
			using Type = std::remove_volatile<T>::type;

			// Handle json primitive types numbers, strings & booleans
			if constexpr (OFS::is_json_compatible<Type>::value) {
				obj = std::move(json.get<Type>());
				return true;
			}
			// Handle objects
			else if constexpr (!OFS::is_json_compatible<Type>::value && !refl::trait::is_container_v<Type>) {
				bool succ = deserializeObject(obj, json);
				return succ;
			}
			// Handle arrays
			else if constexpr (refl::trait::is_container_v<Type>) {
				bool succ = deserializeContainerItems(obj, json);
				return succ;
			}
			else 
			{ static_assert(false); }
			return false;
		}

	private:
		template<typename ObjectType>
		inline static bool serializeObject(const ObjectType& objectRef, nlohmann::json& objectJson) noexcept
		{
			bool successful = true;
			for_each(refl::reflect(objectRef).members, [&](auto member) noexcept
			{
				auto& memberRef = GetConstFieldRef(member, objectRef);
				using MemberType = std::remove_const<std::remove_reference<decltype(memberRef)>::type>::type;
				auto& currentJson = objectJson[get_display_name(member)];
				bool succ = OFS::Serializer::Serialize(memberRef, currentJson);
				if(!succ) successful = false;
			});
			return successful;
		}

		template<typename ItemType>
		inline static bool serializeContainerItems(const std::vector<ItemType>& container, nlohmann::json& jsonArray) noexcept
		{
			for(auto& item : container) {
				auto& jsonItem = jsonArray.emplace_back();
				bool succ = OFS::Serializer::Serialize(item, jsonItem);
				if(!succ) return false;
			}
			return true;
		}

		template<typename ItemType, size_t Size>
		inline static bool serializeContainerItems(const std::array<ItemType, Size>& container, nlohmann::json& jsonArray) noexcept
		{
			for(auto& item : container) {
				auto& jsonItem = jsonArray.emplace_back();
				bool succ = OFS::Serializer::Serialize(item, jsonItem);
				if(!succ) return false;
			}
			return true;
		}

	public:
		template<typename T>
		inline static bool Serialize(const T& obj, nlohmann::json& json) {
			using Type = std::remove_volatile<T>::type;

			// Handle json primitive types numbers, strings & booleans
			if constexpr (OFS::is_json_compatible<Type>::value) {
				json = obj;
				return true;
			}
			// Handle objects
			else if constexpr (!OFS::is_json_compatible<Type>::value && !refl::trait::is_container_v<Type>) {
				bool succ = serializeObject(obj, json);
				return succ;
			}
			// Handle arrays
			else if constexpr (refl::trait::is_container_v<Type>) {
				auto jsonArray = nlohmann::json::array();
				bool succ = serializeContainerItems(obj, jsonArray);
				json = std::move(jsonArray);
				return succ;
			}
			else { static_assert(false); }
			return false;
		}
	};
}