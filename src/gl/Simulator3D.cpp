#include "Simulator3D.h"

#include "OpenFunscripter.h"

#include "imgui.h"

#include "OFS_im3d.h"

#include "glm/gtx/matrix_decompose.hpp"


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

void Simulator3D::reset() noexcept
{
    view = glm::mat4(1.f);
    viewPos = glm::vec3(0.f, 0.f, 0.f);
    view = glm::translate(view, viewPos);

    translation = glm::mat4(1.f);
    translation = glm::translate(translation, glm::vec3(0.f, 0.f, -simDistance));

    lightPos = glm::vec3(0.f, 0.f, 0.f);

    Zoom = 3.f;
}

void Simulator3D::setup() noexcept
{
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

    reset();
}

void Simulator3D::ShowWindow(bool* open) noexcept
{
    if (!*open) { return; }
    auto app = OpenFunscripter::ptr;
    float currentMs = app->player.getCurrentPositionMsInterp();
    float roll = 0.f;
    float pitch = 0.f;
    float yaw = 0.f;
    float scriptPos = 0.f;
    int32_t loadedScriptsCount = app->LoadedFunscripts.size();
    
    bool easing = app->scripting->Overlay()->SplineLines;

    if (posIndex >= 0 && posIndex < loadedScriptsCount) {
        scriptPos = app->LoadedFunscripts[posIndex]->GetPositionAtTime(currentMs, easing);
    }
    if (rollIndex >= 0 && rollIndex < loadedScriptsCount) {
        roll = app->LoadedFunscripts[rollIndex]->GetPositionAtTime(currentMs, easing) - 50.f;
        roll = (rollRange/2.f) * (roll / 50.f);
    }
    if (pitchIndex >= 0 && pitchIndex < loadedScriptsCount) {
        pitch = app->LoadedFunscripts[pitchIndex]->GetPositionAtTime(currentMs, easing) - 50.f;
        pitch = (pitchRange/2.f) * (pitch / 50.f);
    }
    //if (twistIndex >= 0 && twistIndex < loadedScriptsCount) {
    //    yaw = app->LoadedFunscripts[twistIndex]->GetPositionAtTime(currentMs) - 50.f;
    //    yaw = (twistRange/2.f) * (yaw / 50.f);
    //}

    auto viewport = ImGui::GetMainViewport();
    
    float ratio = viewport->Size.x / viewport->Size.y;
    projection = glm::ortho(-Zoom*ratio, Zoom*ratio, -Zoom, Zoom, 0.1f, 100.f);
    
    ImGui::Begin("Simulator 3D", open, ImGuiWindowFlags_None);

    if (ImGui::Button("Reset", ImVec2(-1.f, 0.f))) {
        reset();
    }

    if (ImGui::Button("Move", ImVec2(-1.f, 0.f))) { TranslateEnabled = !TranslateEnabled; }
    glm::mat3 rot(1.f);
    glm::vec3 scale(1.f);
    if (TranslateEnabled) {
        Im3d::Gizmo("Move", glm::value_ptr(translation));
    }

    ImGui::SliderFloat("Distance", &Zoom, 0.1f, MaxZoom);

    // TODO: use more efficient way of doing getting this vector...
    glm::vec3 direction;
    {
        glm::mat4 directionMtx = translation;
        directionMtx = glm::rotate(directionMtx, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
        direction = glm::vec3(directionMtx[1][0], directionMtx[1][1], directionMtx[1][2]);
        direction = glm::normalize(direction);
    }

    if (ImGui::CollapsingHeader("Settings")) {
        ImGui::ColorEdit4("Box", &boxColor.Value.x);
        ImGui::ColorEdit4("Container", &containerColor.Value.x);
        ImGui::InputFloat("Roll deg", &rollRange);
        ImGui::InputFloat("Pitch deg", &pitchRange);
        ImGui::InputFloat("Twist deg", &twistRange);
    }

    auto ScriptCombo = [](auto Id, int32_t* index) {
        auto app = OpenFunscripter::ptr;
        int32_t loadedScriptCount = app->LoadedFunscripts.size();
        if (ImGui::BeginCombo(Id, *index >= 0 && *index < loadedScriptCount ? app->LoadedFunscripts[*index]->metadata.title.c_str() : "None", ImGuiComboFlags_PopupAlignLeft)) {
            if (ImGui::Selectable("None", *index < 0) || ImGui::IsItemHovered()) {
                *index = -1;
            }
            for (int i = 0; i < loadedScriptCount; i++) {
                if (ImGui::Selectable(app->LoadedFunscripts[i]->metadata.title.c_str(), *index == i) || ImGui::IsItemHovered()) {
                    *index = i;
                }
            }
            ImGui::EndCombo();
        }
    };
    ScriptCombo("Position", &posIndex);
    ScriptCombo("Roll", &rollIndex);
    ScriptCombo("Pitch", &pitchIndex);
    //ScriptCombo("Twist", &twistIndex);


    ImGui::End();

    glm::vec3 position; 
    {
        glm::vec3 scale;
        glm::quat orientation;
        glm::vec3 skew;
        glm::vec4 perspec;
        glm::decompose(translation, scale, orientation, position, skew, perspec);
    }
    
    constexpr float antiZBufferFight = 0.005f;

    const float cubeHeight = (simLength + simCubeSize) * ((scriptPos) / 100.f);
    // container model matrix
    containerModel = glm::mat4(1.f);
    containerModel = glm::translate(containerModel, ((direction * ((cubeHeight + antiZBufferFight)/2.f))) + position);
    containerModel = glm::rotate(containerModel, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    containerModel = glm::rotate(containerModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    containerModel = glm::rotate(containerModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    

    containerModel = glm::scale(containerModel, glm::vec3(simCubeSize, (simLength + simCubeSize) - cubeHeight, simCubeSize)*1.01f);

    // box model matrix
    boxModel = glm::mat4(1.f);
    boxModel = glm::translate(boxModel, (-(direction * ((simLength + simCubeSize) - (cubeHeight - antiZBufferFight)))/2.f) + position);
    boxModel = glm::rotate(boxModel, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    boxModel = glm::rotate(boxModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    boxModel = glm::rotate(boxModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    boxModel = glm::scale(boxModel, glm::vec3(simCubeSize, cubeHeight, simCubeSize));
}

void Simulator3D::render() noexcept
{
    if (!OpenFunscripter::ptr->settings->data().show_simulator_3d) return;
    
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

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

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    lightShader->ObjectColor(&containerColor.Value.x);
    lightShader->ModelMtx(glm::value_ptr(containerModel));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);


    glDisable(GL_DEPTH_TEST);
    //glDisable(GL_BLEND);
}
