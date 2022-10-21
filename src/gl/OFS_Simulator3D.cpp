#include "OFS_Simulator3D.h"

#include "OFS_GL.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"

#include "Funscript.h"
#include "OFS_Serialization.h"
#include "OFS_ImGui.h"
#include "OpenFunscripter.h"
#include "OFS_Shader.h"

#include "OFS_Reflection.h"
#include "OFS_StateHandle.h"

// cube pos + normals
constexpr float vertices[] = {
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
};

constexpr float simLength = 2.f;
constexpr float simDistance = 5.f;
constexpr float simCubeSize = 0.5f;

struct Simulator3dState
{
	Serializable<glm::mat4> Translation;
	float Distance = 3.f;
};

REFL_TYPE(Simulator3dState)
	REFL_FIELD(Translation)
	REFL_FIELD(Distance)
REFL_END

OFS_REGISTER_STATE(Simulator3dState);

static inline auto& State(uint32_t stateHandle) noexcept
{
    return OFS_StateHandle<Simulator3dState>(stateHandle).Get();
}

void Simulator3D::Init() noexcept
{
    stateHandle = OFS_StateHandle<Simulator3dState>::Register(Simulator3D::StateName);
	lightShader = std::make_unique<LightingShader>();

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    Reset(true);
}

void Simulator3D::Reset(bool ignoreState) noexcept
{
    view = glm::mat4(1.f);
    viewPos = glm::vec3(0.f, 0.f, 0.f);
    view = glm::translate(view, viewPos);

    if(!ignoreState) {
        auto& state = State(stateHandle);
        state.Translation = glm::mat4(1.f);
        state.Translation = glm::translate(state.Translation.Value, glm::vec3(0.f, 0.f, -simDistance));
        state.Distance = 3.f;
    }

    lightPos = glm::vec3(0.f, 0.f, 0.f);
    ImGuizmo::SetOrthographic(true);
}

void Simulator3D::ShowWindow(bool* open, float currentTime, bool easing, std::vector<std::shared_ptr<Funscript>>& scripts) noexcept
{
    if (open != nullptr && !*open) { return; }
    OFS_PROFILE(__FUNCTION__);
    const int32_t loadedScriptsCount = scripts.size();
    auto viewport = ImGui::GetMainViewport();
    
    auto& state = State(stateHandle);

    float ratio = viewport->Size.x / viewport->Size.y;
    projection = glm::ortho(-state.Distance*ratio, state.Distance*ratio, -state.Distance, state.Distance, 0.1f, 100.f);

    ImGui::Begin(TR_ID("SIMULATOR_3D", Tr::SIMULATOR_3D), open, ImGuiWindowFlags_None);

    if (Editing == IsEditing::No) {
        if (posIndex >= 0 && posIndex < loadedScriptsCount) {
            scriptPos = easing 
                ? scripts[posIndex]->SplineClamped(currentTime)
                : scriptPos = scripts[posIndex]->GetPositionAtTime(currentTime);
        }
        else { scriptPos = 0.f; }
        
        if (RollOverride >= 0) {
            roll = RollOverride - 50.f;
            roll = (rollRange / 2.f) * (roll / 50.f);
            RollOverride = -1.f;
        }
        else if (rollIndex >= 0 && rollIndex < loadedScriptsCount) {
            roll = easing 
                ? scripts[rollIndex]->SplineClamped(currentTime) - 50.f
                : roll = scripts[rollIndex]->GetPositionAtTime(currentTime) - 50.f;
            roll = (rollRange/2.f) * (roll / 50.f);
        }
        else { roll = 0.f; }

        if (PitchOverride >= 0) {
            pitch = PitchOverride - 50.f;
            pitch = (pitchRange / 2.f) * (pitch / 50.f);
            PitchOverride = -1.f;
        }
        else if (pitchIndex >= 0 && pitchIndex < loadedScriptsCount) {
            pitch =  easing 
                ? scripts[pitchIndex]->SplineClamped(currentTime) - 50.f
                : scripts[pitchIndex]->GetPositionAtTime(currentTime) - 50.f;
            pitch = (pitchRange/2.f) * (pitch / 50.f);
        }
        else { pitch = 0.f; }

        if (twistIndex >= 0 && twistIndex < loadedScriptsCount) {
            float spin = easing 
                ? scripts[twistIndex]->SplineClamped(currentTime) - 50.f
                : scripts[twistIndex]->GetPositionAtTime(currentTime) - 50.f;
            yaw = (twistRange/2.f) * (spin / 50.f);
        }
        else { yaw = 0.f; }
    }

    if (ImGui::BeginTabBar("##3D tab bar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem(TR(CONFIGURATION))) {
            if (ImGui::Button(TR(RESET), ImVec2(-1.f, 0.f))) {
                Reset();
            }

            if (ImGui::Button(TR(MOVE), ImVec2(-1.f, 0.f))) { TranslateEnabled = !TranslateEnabled; }
            glm::mat3 rot(1.f);
            glm::vec3 scale(1.f);


            if (TranslateEnabled) {
                auto drawList = ImGui::GetForegroundDrawList(viewport);
                ImGuizmo::SetDrawlist(drawList);
                ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);

                if (ImGuizmo::Manipulate(glm::value_ptr(view),
                    glm::value_ptr(projection),
                    ImGuizmo::OPERATION::TRANSLATE,
                    ImGuizmo::MODE::WORLD,
                    glm::value_ptr(state.Translation.Value), NULL, NULL)) {
                    auto g = ImGui::GetCurrentContext();
                    auto window = ImGui::GetCurrentWindow();
                    g->HoveredWindow = window;
                    g->HoveredDockNode = window->DockNode;
                }
            }

            ImGui::SliderFloat(TR(DISTANCE), &state.Distance, 0.1f, MaxZoom);
            ImGui::SliderAngle(TR(GLOBAL_YAW), &globalYaw, -180.f, 180.f);
            ImGui::SliderAngle(TR(GLOBAL_PITCH), &globalPitch, -90.f, 90.f);

            if (ImGui::CollapsingHeader(TR(SETTINGS))) {
                ImGui::ColorEdit4(TR(BOX), &boxColor.Value.x);
                ImGui::ColorEdit4(TR(CONTAINER), &containerColor.Value.x);
                ImGui::ColorEdit4(TR(TWIST), &twistBoxColor.Value.x);

                ImGui::InputFloat(TR(ROLL_DEG), &rollRange);
                ImGui::InputFloat(TR(PITCH_DEG), &pitchRange);
                ImGui::InputFloat(TR(TWIST_DEG), &twistRange);
            }

            auto ScriptCombo = [](auto Id, int32_t* index, uint32_t loadedScriptsCount, const auto& scripts) noexcept
            {
                if (ImGui::BeginCombo(Id, *index >= 0 && *index < loadedScriptsCount ? scripts[*index]->Title.c_str() : TR(NONE), ImGuiComboFlags_PopupAlignLeft)) {
                    if (ImGui::Selectable(TR(NONE), *index < 0) || ImGui::IsItemHovered()) {
                        *index = -1;
                    }
                    for (int i = 0; i < loadedScriptsCount; i++) {
                        if (ImGui::Selectable(scripts[i]->Title.c_str(), *index == i) || ImGui::IsItemHovered()) {
                            *index = i;
                        }
                    }
                    ImGui::EndCombo();
                }
            };
            ScriptCombo(TR(POSITION), &posIndex, loadedScriptsCount, scripts);
            ScriptCombo(TR(ROLL), &rollIndex, loadedScriptsCount, scripts);
            ScriptCombo(TR(PITCH), &pitchIndex, loadedScriptsCount, scripts);
            ScriptCombo(TR(TWIST), &twistIndex, loadedScriptsCount, scripts);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(TR(EDIT))) {
            auto addEditAction = [](const auto& script, float value, float min, float max) noexcept
            {
                auto app = OpenFunscripter::ptr;
                float range = std::abs(max - min);
                float pos = ((value + std::abs(min)) / range) * 100.f;
                FunscriptAction action(app->player->CurrentTime(), pos);
                script->AddEditAction(action, app->scripting->LogicalFrameTime());
            };

            auto editAxisSlider = [](const char* name, float* value, float min, float max, IsEditing* IsEditing, int* IsEditingIdx, int idx, float scrollStep) 
                noexcept -> bool
            {
                auto& io = ImGui::GetIO();
                if(ImGui::SliderFloat(name, value, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    *IsEditing = IsEditing::ClickDrag;
                    *IsEditingIdx = idx;
                }
                
                bool hovered = ImGui::IsItemHovered();
                if (hovered && io.MouseWheel != 0.f) {
                    float step = std::abs(max - min) * (scrollStep/99.9f);
                    *IsEditing = IsEditing::Mousewheel;
                    *IsEditingIdx = idx;
                    *value = Util::Clamp(*value + io.MouseWheel * step, min, max);
                }
                if ((*IsEditing & IsEditing::ClickDrag) && ImGui::IsItemDeactivated() 
                    || ((*IsEditing & IsEditing::Mousewheel) && !hovered)) {
                    bool CurrentlyBeingEdited = *IsEditingIdx == idx;
                    if (CurrentlyBeingEdited) {
                        *IsEditing = IsEditing::No;
                        *IsEditingIdx = -1;
                        return true;
                    }
                }
                return false;
            };

            bool editOccured = false;
            if (rollIndex >= 0 && rollIndex < loadedScriptsCount) {
                editOccured = editAxisSlider(TR(ROLL), 
                    &roll, -(rollRange / 2.f), (rollRange / 2.f), 
                    &Editing,  &EditingIdx, rollIndex, EditingScrollMultiplier) || editOccured;
            }
            if (pitchIndex >= 0 && pitchIndex < loadedScriptsCount) {
                editOccured = editAxisSlider(TR(PITCH), 
                    &pitch, -(pitchRange / 2.f), (pitchRange / 2.f), 
                    &Editing, &EditingIdx, pitchIndex, EditingScrollMultiplier) || editOccured;
            }
            if (twistIndex >= 0 && twistIndex < loadedScriptsCount) {
                editOccured = editAxisSlider(TR(YAW), 
                    &yaw, -(twistRange / 2.f), (twistRange / 2.f), 
                    &Editing, &EditingIdx, twistIndex, EditingScrollMultiplier) || editOccured;
            }

            if (ImGui::Button(TR(INSERT_CURRENT_POSITION), ImVec2(-1.f, 0.f)) || editOccured) {
                auto app = OpenFunscripter::ptr;
                app->undoSystem->Snapshot(StateType::ADD_EDIT_ACTION);
                if (rollIndex >= 0 && rollIndex < loadedScriptsCount) {
                    addEditAction(scripts[rollIndex], roll, -(rollRange / 2.f), (rollRange / 2.f));
                }
                if (pitchIndex >= 0 && pitchIndex < loadedScriptsCount) {
                    addEditAction(scripts[pitchIndex], pitch, -(pitchRange / 2.f), (pitchRange / 2.f));
                }
                if (twistIndex >= 0 && twistIndex < loadedScriptsCount) {
                    addEditAction(scripts[twistIndex], yaw, -(twistRange / 2.f), (twistRange / 2.f));
                }
            }

            ImGui::Separator();

            if (ImGui::InputFloat(TR(SCROLL_PERCENT), &EditingScrollMultiplier, 1.f, 1.f, "%.3f", ImGuiInputTextFlags_None)) {
                EditingScrollMultiplier = Util::Clamp(EditingScrollMultiplier, 1.f, 20.f);
            }
            OFS::Tooltip(TR(SCROLL_PERCENT_TOOLTIP));
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    {
        glm::mat4 directionMtx = state.Translation;

        directionMtx = glm::rotate(directionMtx, globalYaw, glm::vec3(0.f, 1.f, 0.f));
        directionMtx = glm::rotate(directionMtx, globalPitch, glm::vec3(1.f, 0.f, 0.f));

        directionMtx = glm::rotate(directionMtx, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(pitch), glm::vec3(-1.f, 0.f, 0.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
        direction = glm::vec3(directionMtx[1][0], directionMtx[1][1], directionMtx[1][2]);
        direction = glm::normalize(direction);
    }


    glm::vec3 position; 
    {
        glm::vec3 scale;
        glm::quat orientation;
        glm::vec3 skew;
        glm::vec4 perspec;
        glm::decompose(state.Translation.Value, scale, orientation, position, skew, perspec);
    }
    
    constexpr float antiZBufferFight = 0.005f;

    const float cubeHeight = (simLength + simCubeSize) * ((scriptPos) / 100.f);
    // container model matrix
    containerModel = glm::mat4(1.f);
    containerModel = glm::translate(containerModel, ((direction * ((cubeHeight + antiZBufferFight)/2.f))) + position);
    
    // global
    containerModel = glm::rotate(containerModel, globalYaw, glm::vec3(0.f, 1.f, 0.f));    
    containerModel = glm::rotate(containerModel, globalPitch, glm::vec3(1.f, 0.f, 0.f));
    // local
    containerModel = glm::rotate(containerModel, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    containerModel = glm::rotate(containerModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    
    containerModel = glm::scale(containerModel, glm::vec3(simCubeSize, (simLength + simCubeSize) - cubeHeight, simCubeSize)*1.01f);

    // box model matrix
    boxModel = glm::mat4(1.f);
    boxModel = glm::translate(boxModel, (-(direction * ((simLength + simCubeSize) - (cubeHeight - antiZBufferFight)))/2.f) + position);
    
    // global
    boxModel = glm::rotate(boxModel, globalYaw, glm::vec3(0.f, 1.f, 0.f));
    boxModel = glm::rotate(boxModel, globalPitch, glm::vec3(1.f, 0.f, 0.f));
    // local
    boxModel = glm::rotate(boxModel, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    boxModel = glm::rotate(boxModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    
    boxModel = glm::scale(boxModel, glm::vec3(simCubeSize, cubeHeight, simCubeSize));

    twistBox = glm::mat4(1.f);
    twistBox = glm::translate(twistBox, position + (direction*(cubeHeight-1.25f)));

    twistBox = glm::rotate(twistBox, globalYaw, glm::vec3(0.f, 1.f, 0.f));
    twistBox = glm::rotate(twistBox, globalPitch, glm::vec3(1.f, 0.f, 0.f));

    twistBox = glm::rotate(twistBox, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    twistBox = glm::rotate(twistBox, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    twistBox = glm::rotate(twistBox, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    twistBox = glm::scale(twistBox, glm::vec3(simCubeSize, simCubeSize/4.f, simCubeSize)*1.5f);
}

void Simulator3D::renderSim() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto viewport = ImGui::GetMainViewport();
    glViewport(0, 0, viewport->Size.x, viewport->Size.y);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    lightShader->use();
    lightShader->LightPos(glm::value_ptr(lightPos));
    lightShader->ProjectionMtx(glm::value_ptr(projection));
    lightShader->ViewMtx(glm::value_ptr(view));
    lightShader->ViewPos(glm::value_ptr(viewPos));

    lightShader->ObjectColor(&boxColor.Value.x);
    lightShader->ModelMtx(glm::value_ptr(boxModel));

    // render the cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);


    lightShader->ObjectColor(&containerColor.Value.x);
    lightShader->ModelMtx(glm::value_ptr(containerModel));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);


    lightShader->ObjectColor(&twistBoxColor.Value.x);
    lightShader->ModelMtx(glm::value_ptr(twistBox));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
