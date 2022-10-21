#pragma once
#include "OFS_Util.h"

#include <string>
#include <vector>
#include <any>

class OFS_StateMetadata
{
    public:
    template<typename T>
    static OFS_StateMetadata CreateMetadata() noexcept
    {
        constexpr auto type = refl::reflect<T>();
        OFS_StateMetadata md;
        md.name = type.name.c_str();
        md.creator = &OFS_StateMetadata::createUntyped<T>;
        md.serializer = &OFS_StateMetadata::serializeUntyped<T>;
        md.deserializer = &OFS_StateMetadata::deserializeUntyped<T>;
        return md;
    }
    
    std::any Create() const noexcept { return creator(); }
    std::string_view Name() const noexcept { return name; }
    
    bool Serialize(const std::any& value, nlohmann::json& obj) const noexcept {
        return serializer(value, obj);
    }

    bool Deserialize(std::any& value, const nlohmann::json& obj) const noexcept {
        return deserializer(value, obj);
    }

    private:
    using OFS_StateCreator = std::any(*)() noexcept;
    using OFS_StateSerializer = bool (*)(const std::any&, nlohmann::json&) noexcept;
    using OFS_StateDeserializer = bool (*)(std::any&, const nlohmann::json&) noexcept;

    std::string name;
    OFS_StateCreator creator;
    OFS_StateSerializer serializer;
    OFS_StateDeserializer deserializer;

    template <typename T>
    static std::any createUntyped() noexcept
    {
        T instance{};
        //for_each(refl::reflect<T>().members, [&](auto member) {
        //    if constexpr (refl::descriptor::has_attribute<UiProperty>(member)) {
        //        auto&& prop = refl::descriptor::get_attribute<UiProperty>(member);
        //        if (auto propIter = props.find(member.name.str()); propIter != props.end()) {
        //            member(instance) = prop.parser(propIter->second);
        //        }
        //    }
        //});
        return instance;
    }

    template<typename T>
    static bool serializeUntyped(const std::any& value, nlohmann::json& obj) noexcept
    {
        auto& realValue = std::any_cast<const T&>(value);
        return OFS::Serializer::Serialize(realValue, obj);
    }

    template<typename T>
    static bool deserializeUntyped(std::any& value, const nlohmann::json& obj) noexcept
    {
        auto& realValue = std::any_cast<T&>(value);
        return OFS::Serializer::Deserialize(realValue, obj);
    }
};

class OFS_StateRegistry
{
    public:
    static OFS_StateRegistry& Get() noexcept
    {
        static OFS_StateRegistry instance;
        return instance;
    }

    const OFS_StateMetadata* Find(std::string_view typeName) const noexcept
    {
        auto iter = std::find_if(metadata.begin(), metadata.end(), [&](auto&& x) {
            return x.Name() == typeName;
        });
        if (iter != metadata.end()) {
            return &(*iter);
        }
        return nullptr;
    }

    template<typename T>
    void RegisterState()
    {
        metadata.push_back(OFS_StateMetadata::CreateMetadata<T>());
    }

    private:
    std::vector<OFS_StateMetadata> metadata;
    OFS_StateRegistry() noexcept {}
};

#define OFS_REGISTER_STATE(StateTypeName)\
static int _global_dummy_ ## StateTypeName = (OFS_StateRegistry::Get().RegisterState<StateTypeName>(), 0)

struct OFS_State
{
    std::string Name;
    std::string TypeName;
    std::any State;
};

class OFS_StateManager
{
    private:
    std::vector<OFS_State> State;
    static OFS_StateManager* instance;

    public:
    static void Init() noexcept;
    static void Shutdown() noexcept;
    inline static OFS_StateManager* Get() noexcept { return instance; }

    template<typename T>
    uint32_t Register(const char* name) noexcept
    {
        constexpr auto type = refl::reflect<T>();
        auto it = std::find_if(State.begin(), State.end(),
            [name](auto& item) noexcept {
                return item.Name == name;
            });
        if(it == State.end()) {
            LOGF_DEBUG("Registering new state \"%s\"", name);
            uint32_t Id = State.size();
            State.emplace_back(
                std::move(OFS_State{name, type.name.c_str(), T()})
            );
            return Id;   
        }
        else {
            LOGF_DEBUG("Loading existing state \"%s\"", name);
            uint32_t existingId = std::distance(State.begin(), it);
            return existingId;
        }
    }

    template<typename T>
    T& Get(uint32_t id) noexcept
    {
        auto& item = State[id];
        auto& value = std::any_cast<T&>(item.State);
        return value;
    }

    nlohmann::json SerializeAll() noexcept;
    bool DeserializeAll(const nlohmann::json& state) noexcept;
};
