#include "OFS_TCode.h"
#include "OFS_Util.h"
#include "SDL_thread.h"
#include "OFS_ImGui.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"

#include "imgui.h"

#include "libserialport.h"
#include "libserialport_internal.h"

bool TCodePlayer::openPort(struct sp_port* openthis) noexcept
{
    if (port != nullptr) {
        sp_close(port);
        sp_free_port(port);
        port = nullptr;
    }

    if (sp_get_port_by_name(openthis->name, &port) != SP_OK) {
        LOGF_ERROR("Failed to get port \"%s\"", openthis->description);
        return false;
    }

    if (sp_open(port, sp_mode::SP_MODE_WRITE) != SP_OK) {
        LOGF_ERROR("Failed to open port \"%s\"", port->description);
        sp_free_port(port);
        port = nullptr;
        return false;
    }

    if (sp_set_baudrate(port, 115200) != SP_OK) {
        LOG_ERROR("Failed to set baud rate to 115200.");
        sp_close(port);
        sp_free_port(port);
        port = nullptr;
        return false;
    }

    return true;
}

TCodePlayer::TCodePlayer() noexcept
{

}

TCodePlayer::~TCodePlayer() noexcept
{
    stop();
    save();
}

void TCodePlayer::loadSettings(const std::string& path) noexcept
{
    bool succ;
    auto json = Util::LoadJson(path, &succ);
    if (succ) {
        OFS::serializer::load(this, &json["tcode_player"]);
    }
    loadPath = path;
}

void TCodePlayer::save() noexcept
{
    nlohmann::json json;
    OFS::serializer::save(this, &json["tcode_player"]);
    Util::WriteJson(json, loadPath, true);
}

static struct TCodeThreadData {
    volatile bool requestStop = false;
    bool running = false;
    
    SDL_atomic_t scriptTime = { 0 };
    
    volatile float speed = 1.f;

    TCodePlayer* player = nullptr;
    TCodeChannels* channel = nullptr;
    TCodeProducer* producer = nullptr;
} Thread;

void TCodePlayer::DrawWindow(bool* open, float currentTime) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);

    ImGui::Begin(TR_ID("T_CODE", Tr::T_CODE), open, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo(TR(PORT), &current_port, [](void* data, int idx, const char** out_text) -> bool {
        const char** port_list = (const char**)data;
        *out_text = ((sp_port*)port_list[idx])->description;
        return true;
        }, port_list, port_count);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (port_list != nullptr) {
            sp_free_port_list(port_list);
        }
        if (sp_list_ports(&port_list) == SP_OK) {
            // count ports
            int x = 0;
            LOG_DEBUG("Available serial ports:\n");
            while (port_list[x] != NULL) {
                LOGF_DEBUG("%s\n", port_list[x]->description);
                x++;
            }
            port_count = x;
        }
    }
    ImGui::SameLine();

    if (port_list != nullptr
        && port_list[0] != nullptr
        && current_port < port_count
        && port_list[current_port] != nullptr) {
        if (ImGui::Button(TR(OPEN_PORT), ImVec2(-1.f, 0.f))) {
            openPort(port_list[current_port]);
        }
    }
    if (port != nullptr && ImGui::Button(TR(OPEN_PORT), ImVec2(-1.f, 0.f))) {
        sp_close(port);
        sp_free_port(port);
        port = nullptr;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (ImGui::CollapsingHeader(TR_ID("ChannelLimits", Tr::LIMITS)))
    {
        auto limitsGui = [this](TChannel chan) noexcept {
            char buf[32];
            auto& c = tcode.Get(chan);
            float availWidth = ImGui::GetContentRegionAvail().x;

            ImGui::SetNextItemWidth(availWidth * 0.6f);
            stbsp_snprintf(buf, sizeof(buf),"##%s_Limit", c.Id);
            ImGui::DragIntRange2(buf,
                &c.limits[0], &c.limits[1], 1,
                TCodeChannel::MinChannelValue, TCodeChannel::MaxChannelValue,
                TR(MIN_INT_FMT), TR(MAX_INT_FMT), ImGuiSliderFlags_AlwaysClamp);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(0.3f * availWidth);
            stbsp_snprintf(buf, sizeof(buf), "%-6s (%s)##%s_Enable", TCodeChannels::Aliases[static_cast<int32_t>(chan)][2], c.Id, c.Id);
            ImGui::Checkbox(buf, &c.Enabled);
        };

        ImGui::TextUnformatted(TR(LINEAR_LIMITS));
        limitsGui(TChannel::L0);
        limitsGui(TChannel::L1);
        limitsGui(TChannel::L2);
        limitsGui(TChannel::L3);

        ImGui::Separator();

        ImGui::TextUnformatted(TR(ROTATION_LIMITS));
        limitsGui(TChannel::R0);
        limitsGui(TChannel::R1);
        limitsGui(TChannel::R2);

        ImGui::Separator();

        ImGui::TextUnformatted(TR(VIBRATION_LIMITS));
        limitsGui(TChannel::V0);
        limitsGui(TChannel::V1);
        limitsGui(TChannel::V2);

        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader(TR(GLOBAL_SETTINGS)))
    {
        ImGui::InputFloat(TR(DELAY), (float*)&delay, 0.01f, 0.01f); OFS::Tooltip(TR(DELAY_TOOLTIP));
        ImGui::SliderInt(TR(TCODE_TICKRATE), (int32_t*)&tickrate, 60, 300, "%d", ImGuiSliderFlags_AlwaysClamp); 
        ImGui::Checkbox(TR(SPLINE), &TCodeChannel::SplineMode);
        OFS::Tooltip(TR(SPLINE_TOOLTIP));
        ImGui::SameLine(); ImGui::Checkbox(TR(REMAP), &TCodeChannel::RemapToFullRange);
        OFS::Tooltip(TR(REMAP_TOOLTIP));
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextUnformatted(TR(OUTPUTS));
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    OFS::Tooltip(TR(YOU_CAN_RIGHT_CLICK_SLIDERS_TOOLTIP));

    for (int i = 0; i < tcode.channels.size(); i++) {
        if(i == 4 || i == 7) ImGui::Spacing();
        auto& c = tcode.channels[i];
        if (!c.Enabled) continue;
        auto& p = prod.producers[i];
        ImGui::PushID(i);
        if (OFS::BoundedSliderInt(c.Id, &c.NextTCodeValue, TCodeChannel::MinChannelValue, TCodeChannel::MaxChannelValue, c.limits[0], c.limits[1], "%d", ImGuiSliderFlags_AlwaysClamp));
        if (ImGui::BeginPopupContextItem())
        {
            ImGui::MenuItem(TR(INVERT), NULL, &c.Invert);
            ImGui::MenuItem(TR(REBALANCE), NULL, &c.Rebalance); OFS::Tooltip(TR(REBALANCE_TOOLTIP));
            ImGui::Separator();
            auto activeIdx = prod.GetProd(static_cast<TChannel>(i)).ScriptIdx();
            
            for (int32_t scriptIdx = 0; scriptIdx < prod.LoadedScripts.size(); scriptIdx++) {
                if (auto& script = prod.LoadedScripts[scriptIdx]) {
                    if (ImGui::MenuItem(script->Title.c_str(), NULL, scriptIdx == activeIdx))
                    {
                        if (scriptIdx != activeIdx) { p.SetScript(scriptIdx); }
                        else { p.SetScript(-2); } // -1 for uninitialized & -2 for unset
                        break;
                    }
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine(); ImGui::Text(" " ICON_ARROW_RIGHT " %s", c.LastCommand);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(TCodeChannels::Aliases[i][2]);
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }
    ImGui::Spacing();
    
    if (!Thread.running) {
        // move to the current position
        prod.sync(currentTime, 1.f);
        prod.tick(currentTime, 1.f);
        const char* cmd = tcode.GetCommandSpeed(500);
        if (cmd != nullptr && port != nullptr) {
            int len = strlen(cmd);
            if (sp_blocking_write(port, cmd, len, 0) < SP_OK) {
                LOG_ERROR("Failed to write to serial port.");
            }
        }
    }

	ImGui::End();
}


static int32_t TCodeThread(void* threadData) noexcept {
    TCodeThreadData* data = (TCodeThreadData*)threadData;

    LOG_INFO("T-Code thread started...");

    float currentTime;
    {
        int32_t tmp = SDL_AtomicGet(&data->scriptTime);
        float scriptTime = *((float*)&tmp) - data->player->delay;
        data->producer->sync(scriptTime, data->player->tickrate);
        currentTime = scriptTime;
    }

    constexpr float maxError = 0.03f;
    bool correctError = false;

    auto startTimePoint = std::chrono::high_resolution_clock::now();
    auto currentTimePoint = startTimePoint;

    while (!data->requestStop) {
        float tickrate = data->player->tickrate;
        float tickDurationSeconds = 1.f / tickrate;
        

        float delay = data->player->delay;
        int32_t data_tmp = SDL_AtomicGet(&data->scriptTime);
        float actualScriptTime = (*(float*)&data_tmp) - delay;
        
        std::chrono::duration<float> duration = startTimePoint - currentTimePoint;
        currentTimePoint = std::chrono::high_resolution_clock::now();
        currentTime += (duration.count() * data->speed);
        
        float playbackError = currentTime - actualScriptTime;
        if (std::abs(playbackError) >= maxError && !correctError) {
            // correct error
            correctError = true;
            LOG_DEBUG("T-Code: correcting playback error...");
        }

        if (playbackError >= maxError * 100) {
            currentTime -= playbackError;
            data->producer->sync(currentTime, tickrate);
            LOG_DEBUG("T-Code: resync");
        }

        if (correctError) {
            currentTime -= playbackError * (1.f / tickrate);
            if (std::abs(currentTime - actualScriptTime) < 0.01f) {
                correctError = false;
                LOG_DEBUG("T-Code: playback error corrected!");
            }
        }

        data->producer->tick(currentTime, tickrate);
        
        // update channels
        const char* cmd = data->channel->GetCommand();
        if (cmd != nullptr && data->player->port != nullptr) {
            int len = strlen(cmd);
            if (sp_blocking_write(data->player->port, cmd, len, 0) < SP_OK) {
                LOG_ERROR("Failed to write to serial port.");
            }
        }

        while((duration = std::chrono::high_resolution_clock::now() - currentTimePoint).count() < tickDurationSeconds) {
            /* Spin wait */
            OFS_PAUSE_INTRIN();
            int ms = (duration.count() / 0.001f) - 1;
            if (ms > 0) { SDL_Delay(ms); }
        }
        startTimePoint = std::chrono::high_resolution_clock::now();
    } 
    data->running = false;
    data->requestStop = false;
    LOG_INFO("T-Code thread stopped.");

    return 0;
}

void TCodePlayer::setScripts(std::vector<std::shared_ptr<const Funscript>>&& scripts) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    prod.LoadedScripts = std::move(scripts);
        
    // assume first is always stroke
    if (prod.GetProd(TChannel::L0).ScriptIdx() == -1) {
        prod.GetProd(TChannel::L0).SetScript(0);
    }

    for(int scriptIndex = 0; scriptIndex < prod.LoadedScripts.size(); scriptIndex++) {
        auto& script = prod.LoadedScripts[scriptIndex];
        for (int i=0; i < static_cast<int>(TChannel::TotalCount); i++) {
            if (prod.GetProd(static_cast<TChannel>(i)).ScriptIdx() >= 0 // skip all which are already set
                || prod.GetProd(static_cast<TChannel>(i)).ScriptIdx() == -2) { continue; }  // -2 is deliberatly unset
            auto& aliases = TCodeChannels::Aliases[i];
            for (auto& alias : aliases)
            {
                if (Util::StringEndsWith(script->Title, alias)) {
                    prod.GetProd(static_cast<TChannel>(i)).SetScript(scriptIndex);
                    break;
                }
            }
        }
    }
    prod.SetChannels(&tcode);
}

void TCodePlayer::play(float currentTime, std::vector<std::shared_ptr<const Funscript>>&& scripts) noexcept
{    
    OFS_PROFILE(__FUNCTION__);
    if (!this->port) return;    
    if (!Thread.running) {
        Thread.running = true;
        Thread.player = this;
        Thread.channel = &this->tcode;
        SDL_AtomicSet(&Thread.scriptTime, *((int*)&currentTime));
        Thread.producer = &this->prod;
        tcode.reset();
        
        setScripts(std::move(scripts));
        auto t = SDL_CreateThread(TCodeThread, "TCodePlayer", &Thread);
        SDL_DetachThread(t);
    }
}

void TCodePlayer::stop() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (Thread.running) {
        Thread.requestStop = true;
        while (!Thread.running) { SDL_Delay(1); }
    }
}

void TCodePlayer::sync(float currentTime, float speed) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    Thread.speed = speed;
    SDL_AtomicSet(&Thread.scriptTime, *((int*)&currentTime));
}

void TCodePlayer::reset() noexcept
{
    stop();
    prod.ClearChannels();
}
