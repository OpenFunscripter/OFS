#include "Simulator3D.h"

#include "OpenFunscripter.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

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

    auto app = OpenFunscripter::ptr;
    //auto roll = std::make_unique<Funscript>();
    //roll->open(R"(E:\funscript\multi-axis\crush\Cherry Crush Ball Sucker POV.roll.funscript)");
    //app->LoadedFunscripts.emplace_back(std::move(roll));

    //auto pitch = std::make_unique<Funscript>();
    //pitch->open(R"(E:\funscript\multi-axis\crush\Cherry Crush Ball Sucker POV.pitch.funscript)");
    //app->LoadedFunscripts.emplace_back(std::move(pitch));

    //auto twist = std::make_unique<Funscript>();
    //twist->open(R"(E:\funscript\multi-axis\crush\Cherry Crush Ball Sucker POV.twist.funscript)");
    //app->LoadedFunscripts.emplace_back(std::move(twist));

    auto roll = std::make_unique<Funscript>();
    roll->open(R"(E:\funscript\multi-axis\Kimber Lee - Beautiful Young Cocksucker Takes Load.roll.funscript)");
    app->LoadedFunscripts.emplace_back(std::move(roll));

    auto pitch = std::make_unique<Funscript>();
    pitch->open(R"(E:\funscript\multi-axis\Kimber Lee - Beautiful Young Cocksucker Takes Load.pitch.funscript)");
    app->LoadedFunscripts.emplace_back(std::move(pitch));

    auto twist = std::make_unique<Funscript>();
    twist->open(R"(E:\funscript\multi-axis\Kimber Lee - Beautiful Young Cocksucker Takes Load.twist.funscript)");
    app->LoadedFunscripts.emplace_back(std::move(twist));
}

void Simulator3D::render() noexcept
{
    auto app = OpenFunscripter::ptr;

    float color[4] { 1.0f, 0.5f, 0.31f, 1.f };
    float colorContainer[4]{ 0.5f, 0.5f, 1.f, 0.4f };

    auto viewport = ImGui::GetMainViewport();
    //const float hCenter = viewport->Size.x / 2.f;
    //const float vCenter = viewport->Size.y / 2.f;
    const float length = 3.f;
    float pos[3] {0.f, 0.f, -1.f };

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

    glEnable(GL_DEPTH_TEST);

    lightShader->use();
    lightShader->LightPos(&pos[0]);

    // view/projection transformations
    glm::mat4 projection = glm::perspective(glm::radians(90.f), viewport->Size.x / viewport->Size.y, 0.1f, 100.0f); /*glm::ortho(0.f, viewport->Size.x, viewport->Size.y, 0.f, 0.1f, 100.f);*/
    glm::mat4 view = glm::mat4(1.f);
    lightShader->ProjectionMtx(glm::value_ptr(projection));
    lightShader->ViewMtx(glm::value_ptr(view));

    constexpr float distance = 3.f;
    constexpr float cubeSize = 0.75f;

    glm::mat4 boxModel(1.f);
    boxModel = glm::mat4(1.f);
    boxModel = glm::translate(boxModel, glm::vec3(0.f, 0.f, -distance));
    boxModel = glm::rotate(boxModel, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    boxModel = glm::rotate(boxModel, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    boxModel = glm::rotate(boxModel, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    glm::vec3 direction = glm::vec3(boxModel[1][0], boxModel[1][1], boxModel[1][2]);
    direction = glm::normalize(direction);
    boxModel = glm::scale(boxModel, glm::vec3(cubeSize * 1.001f, length + cubeSize, cubeSize * 1.001f));



    glm::mat4 model = glm::mat4(1.0f);
    float cubeHeight = length * ((scriptPos) / 100.f);

    //model = glm::translate(model, (direction * cubeHeight) - glm::vec3(0.f, 0.f, distance)/*  glm::vec3(0.f, ((cubeHeight)-(length/2.f)), -distance)*/);
    model = glm::translate(model, (direction * (cubeHeight/2.f)) + glm::vec3(0.f, 0.f, -distance) - (direction * ((length+(cubeSize))/2.f)));
    model = glm::rotate(model, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    model = glm::rotate(model, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    model = glm::scale(model, glm::vec3(cubeSize, cubeHeight, cubeSize));
    //model = glm::scale(model, glm::vec3(cubeSize));
    lightShader->ObjectColor(&color[0]);
    lightShader->ModelMtx(glm::value_ptr(model));

    // render the cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /*model = glm::mat4(1.f);
    model = glm::translate(model, glm::vec3(0.f, 0.f, -distance));
    model = glm::rotate(model, glm::radians(roll), glm::vec3(0.f, 0.f, 1.f));
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    model = glm::rotate(model, glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    model = glm::scale(model, glm::vec3(cubeSize*1.001f, length + cubeSize, cubeSize*1.001f));*/

    lightShader->ObjectColor(colorContainer);
    lightShader->ModelMtx(glm::value_ptr(boxModel));
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);


    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
