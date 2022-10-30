#pragma once
#include "OFS_Util.h"
#include "OFS_Serialization.h"

#include <string>
#include <vector>
#include <any>
#include <map>

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

#define OFS_REGISTER_STATE(StateTypeName) OFS_StateRegistry::Get().RegisterState<StateTypeName>()

struct OFS_State
{
    std::string Name;
    std::string TypeName;
    const OFS_StateMetadata* Metadata = nullptr;
    std::any State;
};

class OFS_StateManager
{
    public:
    using StateHandleMap = std::map<std::string, uint32_t>;
    private:
    std::vector<OFS_State> ApplicationState;
    std::vector<OFS_State> ProjectState;

    StateHandleMap ApplicationHandleMap;
    StateHandleMap ProjectHandleMap;

    static OFS_StateManager* instance;

    template<typename T>
    inline static uint32_t registerState(const char* name, std::vector<OFS_State>& stateCollection, StateHandleMap& handleMap) noexcept
    {
        constexpr auto type = refl::reflect<T>();
        auto it = handleMap.find(name);
        if(it == handleMap.end()) {
            LOGF_DEBUG("Registering new state \"%s\". Type: %s", name, type.name.c_str());

            auto metadata = OFS_StateRegistry::Get().Find(type.name.c_str());
            FUN_ASSERT(metadata, "State wasn't registered using OFS_REGISTER_STATE macro");

            uint32_t Id = stateCollection.size();
            stateCollection.emplace_back(
                std::move(OFS_State{name, type.name.c_str(), metadata, std::move(std::make_any<T>())})
            );

            auto sanityCheck = handleMap.insert(std::make_pair(std::string(name), Id));
            FUN_ASSERT(sanityCheck.second, "Why did this fail?");

            return Id;   
        }
        else {
            FUN_ASSERT(stateCollection[it->second].Name == name, "Something went wrong");
            LOGF_DEBUG("Loading existing state \"%s\"", name);
            return it->second;
        }
    }

    template<typename T>
    inline static T& getState(uint32_t id, std::vector<OFS_State>& stateCollection) noexcept
    {
        FUN_ASSERT(id < stateCollection.size(), "out of bounds");
        auto& item = stateCollection[id];
        auto& value = std::any_cast<T&>(item.State);
        return value;
    }

    public:
    static void Init() noexcept;
    static void Shutdown() noexcept;
    inline static OFS_StateManager* Get() noexcept { return instance; }

    template<typename T>
    inline uint32_t RegisterApp(const char* name) noexcept
    {
        return registerState<T>(name, ApplicationState, ApplicationHandleMap);
    }

    template<typename T>
    inline uint32_t RegisterProject(const char* name) noexcept
    {
        return registerState<T>(name, ProjectState, ProjectHandleMap);
    }

    template<typename T>
    inline T& GetApp(uint32_t id) noexcept
    {
        return getState<T>(id, ApplicationState);
    }

    template<typename T>
    inline T& GetProject(uint32_t id) noexcept
    {
        return getState<T>(id, ProjectState);
    }

    nlohmann::json SerializeAppAll() noexcept;
    bool DeserializeAppAll(const nlohmann::json& state) noexcept;

    nlohmann::json SerializeProjectAll() noexcept;
    bool DeserializeProjectAll(const nlohmann::json& project) noexcept;
};
