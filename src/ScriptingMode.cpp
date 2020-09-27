#include "ScriptingMode.h"

#include "OpenFunscripter.h"

#include "imgui.h"

ScripingModeBaseImpl::ScripingModeBaseImpl()
{
}

inline Funscript& ScripingModeBaseImpl::ctx() {
    return OpenFunscripter::script();
}

void ScriptingMode::setup()
{
    ctx = OpenFunscripter::ptr;
    setMode(DEFAULT_MODE);
}

void ScriptingMode::DrawScriptingMode(bool* open)
{
	ImGui::Begin("Scripting Mode", open);
    ImGui::PushItemWidth(-1);
    // ATTENTION: order needs to be the same as the enum
    if (ImGui::Combo("##Mode", (int*)&active_mode, 
        "Default\0"
        "Alternating\0"
        "Dynamic injection\0"
        "\0")) {
        setMode(active_mode);
    }
    Util::Tooltip("Scripting mode");
    impl->DrawModeSettings();
    ImGui::PopItemWidth();
	ImGui::End();
}

void ScriptingMode::setMode(ScriptingModeEnum mode)
{
    active_mode = mode;
    switch (mode) {
    case ALTERNATING:
    {
        impl = std::make_unique<AlternatingImpl>();
        break;
    }
    case DYNAMIC_INJECTION:
    {
        impl = std::make_unique<DynamicInjectionImpl>();
        break;
    }
    default:
    {
        impl = std::make_unique<DefaultModeImpl>();
        break;
    }
    }
}

void ScriptingMode::addEditAction(const FunscriptAction& action)
{
    auto ptr = ctx->LoadedFunscript->GetActionAtTime(action.at, ctx->player.getFrameTimeMs());
    if (ptr != nullptr) {
        ctx->LoadedFunscript->EditAction(*ptr, FunscriptAction(ptr->at, action.pos));
    }
    else {
	    impl->addAction(action);
    }
}

// dynamic top injection
void DynamicInjectionImpl::DrawModeSettings()
{
    ImGui::SliderFloat("##Target speed (units/s)", &target_speed, min_speed, max_speed);
    Util::Tooltip("Target speed (units/s)");
    target_speed = std::round(Util::Clamp(target_speed, min_speed, max_speed));

    ImGui::SliderFloat("##Up/Down speed bias", &direction_bias, -0.50f, 0.50f);
    Util::Tooltip("Up/Down speed bias");

    ImGui::Columns(2, 0, false);
    if (ImGui::RadioButton("Top", top_bottom_direction == 1)) {
        top_bottom_direction = 1;
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton("Bottom", top_bottom_direction == -1)) {
        top_bottom_direction = -1;
    }
    ImGui::NextColumn();

}

// dynamic injection
void DynamicInjectionImpl::addAction(const FunscriptAction& action)
{
    auto previous = ctx().GetPreviousActionBehind(action.at);
    if (previous != nullptr) {
        int32_t inject_at = previous->at + ((action.at - previous->at) / 2) + (((action.at - previous->at) / 2) * direction_bias);

        int32_t inject_duration = inject_at - previous->at;
        int32_t inject_pos = Util::Clamp<int32_t>(previous->pos + (top_bottom_direction * (inject_duration / 1000.0) * target_speed), 0.0, 100.0);
        ctx().AddAction(FunscriptAction(inject_at, inject_pos));
    }
    ctx().AddAction(action);
}

// alternating
void AlternatingImpl::DrawModeSettings()
{
    ImGui::Checkbox("Fixed range", &fixed_range_enabled);
    if (fixed_range_enabled) {
        bool input_active = false;
        
        ImGui::LabelText("BottomLabel", "%s", "Fixed bottom");
        ImGui::InputInt("Fixed bottom", &fixed_bottom, 1, 100);
        input_active = input_active || ImGui::IsItemActive();
        
        ImGui::LabelText("TopLabel", "%s", "Fixed top");
        ImGui::InputInt("Fixed top", &fixed_top);
        input_active = input_active || ImGui::IsItemActive();

        fixed_bottom = Util::Clamp<int>(fixed_bottom, 0, 100);
        fixed_top = Util::Clamp<int>(fixed_top, 0, 100);
        
        if (fixed_bottom > fixed_top && !input_active)
        {
            // correct user error :^)
            auto tmp = fixed_bottom;
            fixed_bottom = fixed_top;
            fixed_top = tmp;
        }
    }
}

void AlternatingImpl::addAction(const FunscriptAction& action)
{
    auto previous = ctx().GetPreviousActionBehind(action.at);
    if (fixed_range_enabled) {
        if (previous != nullptr) {
            if (previous->pos >= fixed_top)
            {
                ctx().AddAction(FunscriptAction(action.at, fixed_bottom));
            }
            else {
                ctx().AddAction(FunscriptAction(action.at, fixed_top));
            }
        }
        else {
            if (std::abs(action.pos - fixed_bottom) > std::abs(action.pos - fixed_top))
                ctx().AddAction(FunscriptAction(action.at, fixed_top));
            else
                ctx().AddAction(FunscriptAction(action.at, fixed_bottom));
        }
        return;
    }

    if (previous != nullptr) {

        if (action.pos >= 50) {
            if (previous->pos - action.pos >= 0) {
                ctx().AddAction(FunscriptAction(action.at, 100 - action.pos));
            }
            else {
                ctx().AddAction(action);
            }
        }
        else {
            if (action.pos - previous->pos >= 0) {
                ctx().AddAction(FunscriptAction(action.at, 100 - action.pos));
            }
            else {
                ctx().AddAction(action);
            }
        }
        return;
    }
    ctx().AddAction(action);
}
