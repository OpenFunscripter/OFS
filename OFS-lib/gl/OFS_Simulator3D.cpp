#include "OFS_Simulator3D.h"

#include "glad/glad.h"
#include "imgui.h"
#include "ImGuizmo.h"

#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/rotate_vector.hpp"


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
    ImGuizmo::SetOrthographic(true);
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

void Simulator3D::ShowWindow(bool* open, int32_t currentMs, bool easing, const std::vector<std::shared_ptr<Funscript>>& scripts) noexcept
{
    if (open != nullptr && !*open) { return; }

    

    const int32_t loadedScriptsCount = scripts.size();


    auto viewport = ImGui::GetMainViewport();
    
    float ratio = viewport->Size.x / viewport->Size.y;
    projection = glm::ortho(-Zoom*ratio, Zoom*ratio, -Zoom, Zoom, 0.1f, 100.f);
    
    ImGui::Begin("Simulator 3D", open, ImGuiWindowFlags_None);

    if (posIndex >= 0 && posIndex < loadedScriptsCount) {
        scriptPos = easing 
            ? scripts[posIndex]->SplineClamped(currentMs)
            : scriptPos = scripts[posIndex]->GetPositionAtTime(currentMs);
    }
    if (rollIndex >= 0 && rollIndex < loadedScriptsCount) {
        roll = easing 
            ? scripts[rollIndex]->SplineClamped(currentMs) - 50.f
            : roll = scripts[rollIndex]->GetPositionAtTime(currentMs) - 50.f;
        roll = (rollRange/2.f) * (roll / 50.f);
    }
    if (pitchIndex >= 0 && pitchIndex < loadedScriptsCount) {
        pitch =  easing 
            ? scripts[pitchIndex]->SplineClamped(currentMs) - 50.f
            : scripts[pitchIndex]->GetPositionAtTime(currentMs) - 50.f;
        pitch = (pitchRange/2.f) * (pitch / 50.f);
    }
    if (twistIndex >= 0 && twistIndex < loadedScriptsCount) {
        float spin = ((easing 
            ? scripts[twistIndex]->SplineClamped(currentMs) - 50.f
            : scripts[twistIndex]->GetPositionAtTime(currentMs) - 50.f) / 50.f) * twistSpeed;

        yaw += spin;
    }

    //roll = glm::radians(roll);
    //ImGui::SliderAngle("Roll", &roll, -rollRange/2.f, rollRange/2.f);
    //roll = glm::degrees(roll);

    //pitch = glm::radians(pitch);
    //ImGui::SliderAngle("Pitch", &pitch, -pitchRange/2.f, pitchRange/2.f);
    //pitch = glm::degrees(pitch);

    //yaw = glm::radians(yaw);
    //ImGui::SliderAngle("Yaw", &yaw, -180.f, 180.f);
    //yaw = glm::degrees(yaw);
    
    //static float twist = 0.f;
    //twist = glm::radians(twist);
    //ImGui::SliderAngle("Twist", &twist, -3, 3);
    //twist = glm::degrees(twist);
    //yaw += twist;


    if (ImGui::Button("Reset", ImVec2(-1.f, 0.f))) {
        reset();
    }

    if (ImGui::Button("Move", ImVec2(-1.f, 0.f))) { TranslateEnabled = !TranslateEnabled; }
    glm::mat3 rot(1.f);
    glm::vec3 scale(1.f);


    if (TranslateEnabled) {
        auto draw_list = ImGui::GetForegroundDrawList();
        ImGuizmo::SetDrawlist(draw_list);
        ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);
        if (ImGuizmo::Manipulate(glm::value_ptr(view),
            glm::value_ptr(projection),
            ImGuizmo::OPERATION::TRANSLATE,
            ImGuizmo::MODE::WORLD,
            glm::value_ptr(translation), NULL, NULL)) {
            auto g = ImGui::GetCurrentContext();
            auto window = ImGui::GetCurrentWindow();
            g->HoveredRootWindow = window;
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
        }
    }

    ImGui::SliderFloat("Distance", &Zoom, 0.1f, MaxZoom);

    // TODO: use more efficient way of doing getting this vector...
    glm::vec3 direction(0.f, -1.f, 0.f);
    {
        glm::mat4 directionMtx = translation;
        directionMtx = glm::rotate(directionMtx, glm::radians(roll), glm::vec3(0.f, 0.f,-1.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(pitch),glm::vec3(1.f, 0.f, 0.f));
        directionMtx = glm::rotate(directionMtx, glm::radians(yaw),  glm::vec3(0.f, 1.f, 0.f));
        direction = glm::vec3(directionMtx[1][0], directionMtx[1][1], directionMtx[1][2]);
        direction = glm::normalize(direction);
    }

    if (ImGui::CollapsingHeader("Settings")) {
        ImGui::ColorEdit4("Box", &boxColor.Value.x);
        ImGui::ColorEdit4("Container", &containerColor.Value.x);
        ImGui::InputFloat("Roll deg", &rollRange);
        ImGui::InputFloat("Pitch deg", &pitchRange);
        ImGui::InputFloat("Twist speed", &twistSpeed);
    }

    auto ScriptCombo = [&](auto Id, int32_t* index) {
        //auto app = OpenFunscripter::ptr;
        if (ImGui::BeginCombo(Id, *index >= 0 && *index < loadedScriptsCount ? scripts[*index]->metadata.title.c_str() : "None", ImGuiComboFlags_PopupAlignLeft)) {
            if (ImGui::Selectable("None", *index < 0) || ImGui::IsItemHovered()) {
                *index = -1;
            }
            for (int i = 0; i < loadedScriptsCount; i++) {
                if (ImGui::Selectable(scripts[i]->metadata.title.c_str(), *index == i) || ImGui::IsItemHovered()) {
                    *index = i;
                }
            }
            ImGui::EndCombo();
        }
    };
    ScriptCombo("Position", &posIndex);
    ScriptCombo("Roll", &rollIndex);
    ScriptCombo("Pitch", &pitchIndex);
    ScriptCombo("Twist", &twistIndex);


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
    containerModel = glm::rotate(containerModel, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    containerModel = glm::rotate(containerModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    //containerModel = glm::rotate(containerModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));    
    containerModel = glm::scale(containerModel, glm::vec3(simCubeSize, (simLength + simCubeSize) - cubeHeight, simCubeSize)*1.01f);

    // box model matrix
    boxModel = glm::mat4(1.f);
    boxModel = glm::translate(boxModel, (-(direction * ((simLength + simCubeSize) - (cubeHeight - antiZBufferFight)))/2.f) + position);
    boxModel = glm::rotate(boxModel, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    boxModel = glm::rotate(boxModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    //boxModel = glm::rotate(boxModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    boxModel = glm::scale(boxModel, glm::vec3(simCubeSize, cubeHeight, simCubeSize));

    boxy = glm::mat4(1.f);
    boxy = glm::translate(boxy, position + (direction*(cubeHeight-1.25f)));
    boxy = glm::rotate(boxy, glm::radians(roll), glm::vec3(0.f, 0.f, -1.f));
    boxy = glm::rotate(boxy, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    boxy = glm::rotate(boxy, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    boxy = glm::scale(boxy, glm::vec3(simCubeSize, simCubeSize/4.f, simCubeSize)*1.5f);
}

void Simulator3D::renderSim() noexcept
{
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


    lightShader->ObjectColor(&red.Value.x);
    lightShader->ModelMtx(glm::value_ptr(boxy));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
