#include "OFS_StateManager.h"

OFS_StateManager* OFS_StateManager::instance = nullptr;

void OFS_StateManager::Init() noexcept
{
    if(!OFS_StateManager::instance) {
        OFS_StateManager::instance = new OFS_StateManager();
    }
}

void OFS_StateManager::Shutdown() noexcept
{
    if(OFS_StateManager::instance) {
        delete OFS_StateManager::instance;
        OFS_StateManager::instance = nullptr;
    }
}


inline static nlohmann::json SerializeStateCollection(const std::vector<OFS_State>& stateCollection) noexcept
{
    nlohmann::json obj;
    for(auto& state : stateCollection) {
        auto md = state.Metadata;
        FUN_ASSERT(md, "metadata was null");

        if(md) {
            auto& subObj = obj[state.Name];
            subObj["TypeName"] = state.TypeName;

            bool succ = md->Serialize(state.State, subObj["State"]);
            if(!succ) {
                LOGF_ERROR("Failed to serialize \"%s\" state. Type: %s", state.Name.c_str(), state.TypeName.c_str());
            }
        }
    }
    return obj;
}

inline static bool DeserializeStateCollection(const nlohmann::json& state, std::vector<OFS_State>& stateCollection, OFS_StateManager::StateHandleMap& handleMap) noexcept
{
    for(auto& stateItem : state.items()) {
        auto& stateValue = stateItem.value();
        if(!stateValue.contains("TypeName") || !stateValue["TypeName"].is_string()) {
            LOG_ERROR("Failed to deserialize expected \"TypeName\"");
            continue;
        }
        else if(!stateValue.contains("State") || stateValue["State"].is_null()) {
            LOG_ERROR("Failed to deserialize expected \"State\"");
        }

        OFS_State state;
        state.Name = stateItem.key();
        state.TypeName = stateValue["TypeName"];

        auto md = OFS_StateRegistry::Get().Find(state.TypeName);
        if(!md) {
            LOGF_ERROR("Didn't find state metadata for \"%s\"", state.TypeName.c_str());
            continue;
        }
        state.Metadata = md;
        state.State = md->Create();
        if(md->Deserialize(state.State, stateValue["State"])) {
            auto handleIt = handleMap.find(state.Name);
            if(handleIt != handleMap.end()) {
                uint32_t handle = handleIt->second;
                if(stateCollection.size() < handle + 1) {
                    stateCollection.resize(handle + 1);
                }
                stateCollection[handle] = std::move(state);
            }
            else {
                uint32_t handle = stateCollection.size();
                handleMap.insert(std::make_pair(state.Name, handle));
                stateCollection.emplace_back(std::move(state));
            }
        }
    }
    return true;
}

nlohmann::json OFS_StateManager::SerializeAppAll() noexcept
{
    return SerializeStateCollection(ApplicationState);
}

bool OFS_StateManager::DeserializeAppAll(const nlohmann::json& state) noexcept
{
    ApplicationState.clear();
    return DeserializeStateCollection(state, ApplicationState, ApplicationHandleMap);
}

nlohmann::json OFS_StateManager::SerializeProjectAll() noexcept
{
    return SerializeStateCollection(ProjectState);
}

bool OFS_StateManager::DeserializeProjectAll(const nlohmann::json& project) noexcept
{
    ProjectState.clear();
    return DeserializeStateCollection(project, ProjectState, ProjectHandleMap);
}