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

nlohmann::json OFS_StateManager::SerializeAll() noexcept
{
    nlohmann::json obj;
    for(auto& state : State) {
        auto md = state.Metadata;
        FUN_ASSERT(md, "metadata was null");

        if(md) {
            auto& subObj = obj[state.Name];
            subObj["TypeName"] = state.TypeName;

            bool succ = md->Serialize(state.State, subObj["State"]);
            if(!succ) {
                LOGF_ERROR("Failed to serialize \"%s\" state. Type: %s", state.Name, state.TypeName);
            }
        }
    }
    return obj;
}

bool OFS_StateManager::DeserializeAll(const nlohmann::json& state) noexcept
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

        auto md = state.Metadata;
        if(!md) {
            LOGF_ERROR("Didn't find state metadata for \"%s\"", state.TypeName.c_str());
            continue;
        }
        state.State = md->Create();
        if(md->Deserialize(state.State, stateValue["State"])) {
            State.emplace_back(std::move(state));
        }
    }
    return true;
}