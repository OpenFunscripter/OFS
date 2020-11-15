#include "Simulator3D.h"

#include "OpenFunscripter.h"

#include "imgui.h"
#include "ImGuizmo.h"

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


constexpr float length = 3.f;
constexpr float distance = 3.f;
constexpr float cubeSize = 0.75f;

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

    view = glm::mat4(1.f);
    //view = glm::translate(view, glm::vec3(0.f, 0.f, 0.f));

    translation = glm::mat4(1.f);
    translation = glm::translate(translation, glm::vec3(0.f, 0.f, -distance));
}

void Simulator3D::ShowWindow(bool* open) noexcept
{
    auto app = OpenFunscripter::ptr;
    float currentMs = app->player.getCurrentPositionMsInterp();
    float scriptPos = app->ActiveFunscript()->GetPositionAtTime(currentMs);
    float roll = 0.f;
    float pitch = 0.f;
    float yaw = 0.f;
    if (app->LoadedFunscripts.size() > 1) {
        roll = app->LoadedFunscripts[1]->GetPositionAtTime(currentMs) - 50.f;
        roll = 30.f * (roll / 50.f);
    }
    if (app->LoadedFunscripts.size() > 2) {
        pitch = app->LoadedFunscripts[2]->GetPositionAtTime(currentMs) - 50.f;
        pitch = 45.f * (pitch / 50.f);
    }
    if (app->LoadedFunscripts.size() > 3) {
        yaw = app->LoadedFunscripts[3]->GetPositionAtTime(currentMs) - 50.f;
        yaw = 90.f * (yaw / 50.f);
    }

    constexpr float length = 3.f;
    constexpr float distance = 3.f;
    constexpr float cubeSize = 0.75f;

    auto viewport = ImGui::GetMainViewport();
    projection = glm::perspective(glm::radians(80.f), viewport->Size.x / viewport->Size.y, 0.1f, 100.0f); 
    


    ImGui::Begin("Simulator 3D", open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking);
    ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        Simulator3D* ctx = (Simulator3D*)cmd->UserCallbackData;
        ctx->render();
    }, this);
    ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    //ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);

    //float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    //ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(boxModel), matrixTranslation, matrixRotation, matrixScale);
    //ImGui::InputFloat3("Tr", matrixTranslation);
    //ImGui::InputFloat3("Rt", matrixRotation);
    //ImGui::InputFloat3("Sc", matrixScale);
    //ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, glm::value_ptr(boxModel));

    ImGuizmo::Manipulate(glm::value_ptr(view),
        glm::value_ptr(projection),
        ImGuizmo::OPERATION::BOUNDS,
        ImGuizmo::MODE::LOCAL,
        glm::value_ptr(translation), NULL, NULL);
    //ImGuizmo::ViewManipulate(glm::value_ptr(view), 1.f, ImVec2(0.f, 0.f), ImVec2(300.f, 300.f), IM_COL32(0, 0, 0, 255));
    ImGui::End();

    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(translation), matrixTranslation, matrixRotation, matrixScale);


    // container model matrix
    containerModel = glm::mat4(1.f);
    containerModel = glm::translate(containerModel, glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]));
    containerModel = glm::rotate(containerModel, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    containerModel = glm::rotate(containerModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    containerModel = glm::rotate(containerModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    
    glm::vec3 direction = glm::vec3(boxModel[1][0], boxModel[1][1], boxModel[1][2]);
    direction = glm::normalize(direction);

    containerModel = glm::scale(containerModel, glm::vec3(cubeSize * 1.05f, length + cubeSize, cubeSize * 1.05f));


    // box model matrix
    boxModel = glm::mat4(1.f);
    const float cubeHeight = length * ((scriptPos) / 100.f);
    boxModel = glm::translate(boxModel, (direction * (cubeHeight/2.f)) + glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]) - (direction * ((length+(cubeSize))/2.f)));
    boxModel = glm::rotate(boxModel, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    boxModel = glm::rotate(boxModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    boxModel = glm::rotate(boxModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    boxModel = glm::scale(boxModel, glm::vec3(cubeSize, cubeHeight, cubeSize));
}

void Simulator3D::render() noexcept
{
    constexpr float color[4] { 1.0f, 0.5f, 0.31f, 1.f };
    constexpr float colorContainer[4]{ 0.5f, 0.5f, 1.f, 0.4f };

    constexpr float pos[3] {0.f, 0.f, -1.f };

    glEnable(GL_DEPTH_TEST);

    lightShader->use();
    lightShader->LightPos(&pos[0]);
    lightShader->ProjectionMtx(glm::value_ptr(projection));
    lightShader->ViewMtx(glm::value_ptr(view));


    //model = glm::scale(model, glm::vec3(cubeSize));
    lightShader->ObjectColor(&color[0]);
    lightShader->ModelMtx(glm::value_ptr(boxModel));

    // render the cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    lightShader->ObjectColor(colorContainer);
    lightShader->ModelMtx(glm::value_ptr(containerModel));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);


    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
