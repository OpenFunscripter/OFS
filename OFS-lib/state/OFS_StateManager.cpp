#include "OFS_StateManager.h"

#include "SDL_timer.h"

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


inline static nlohmann::json SerializeStateCollection(const std::vector<OFS_State>& stateCollection, bool enableBinary) noexcept
{
    auto startTime = SDL_GetPerformanceCounter();
    nlohmann::json obj;
    for(auto& state : stateCollection) {
        auto md = state.Metadata;
        FUN_ASSERT(md, "metadata was null");

        if(md) {
            auto& subObj = obj[state.Name];
            subObj["TypeName"] = state.TypeName;

            bool succ = md->Serialize(state.State, subObj["State"], enableBinary);
            if(!succ) {
                LOGF_ERROR("Failed to serialize \"%s\" state. Type: %s", state.Name.c_str(), state.TypeName.c_str());
            }
        }
    }

    auto duration = (float)(SDL_GetPerformanceCounter() - startTime) / (float)SDL_GetPerformanceFrequency();
    LOGF_INFO("OFS_StateManager::SerializeStateCollection took %f seconds", duration);
    return obj;
}

inline static bool DeserializeStateCollection(const nlohmann::json& state, std::vector<OFS_State>& stateCollection, OFS_StateManager::StateHandleMap& handleMap, bool enableBinary) noexcept
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
        if(md->Deserialize(state.State, stateValue["State"], enableBinary)) {
            auto handleIt = handleMap.find(state.Name);
            if(handleIt != handleMap.end()) {
                uint32_t handle = handleIt->second.second;
                if(stateCollection.size() < handle + 1) {
                    stateCollection.resize(handle + 1);
                }
                stateCollection[handle] = std::move(state);
            }
            else {
                uint32_t handle = stateCollection.size();
                handleMap.insert(std::make_pair(state.Name, std::make_pair(md->Name(), handle)));
                stateCollection.emplace_back(std::move(state));
            }
        }
    }

    // Ideally this does nothing because every item has a value.
    for(auto& state : handleMap)
    {
        uint32_t handle = state.second.second;
        if(stateCollection.size() < handle + 1) {
            stateCollection.resize(handle + 1);
        }
        if(!stateCollection[handle].State.has_value()) {
            // Default initialize
            auto md = OFS_StateRegistry::Get().Find(state.second.first);
            FUN_ASSERT(md, "Metadata not found this must not happen in this context.");
            auto& stateItem = stateCollection[handle];
            stateItem.Name = state.first;
            stateItem.State = md->Create();
            stateItem.Metadata = md;
            stateItem.TypeName = md->Name();
        }
    }

    return true;
}

nlohmann::json OFS_StateManager::SerializeAppAll(bool enableBinary) noexcept
{
    return SerializeStateCollection(ApplicationState, enableBinary);
}

bool OFS_StateManager::DeserializeAppAll(const nlohmann::json& state, bool enableBinary) noexcept
{
    ApplicationState.clear();
    return DeserializeStateCollection(state, ApplicationState, ApplicationHandleMap, enableBinary);
}

nlohmann::json OFS_StateManager::SerializeProjectAll(bool enableBinary) noexcept
{
    return SerializeStateCollection(ProjectState, enableBinary);
}

bool OFS_StateManager::DeserializeProjectAll(const nlohmann::json& project, bool enableBinary) noexcept
{
    ProjectState.clear();
    return DeserializeStateCollection(project, ProjectState, ProjectHandleMap, enableBinary);
}

void OFS_StateManager::ClearProjectAll() noexcept
{
    ProjectState.clear();
    // Initialize with defaults
    DeserializeStateCollection(nlohmann::json::object(), ProjectState, ProjectHandleMap, false);
}