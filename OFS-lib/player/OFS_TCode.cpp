#include "OFS_TCode.h"
#include "OFS_Util.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "SDL.h"
#include "OFS_ImGui.h"
#include "OFS_Profiling.h"

#include "implot.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "libserialport.h"
#include "libserialport_internal.h"

#include <chrono>

// utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer() {
        MaxSize = 2000;
        Offset = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x, y));
        else {
            Data[Offset] = ImVec2(x, y);
            Offset = (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset = 0;
        }
    }
};

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

TCodePlayer::TCodePlayer()
{
#ifndef NDEBUG
    if(ImPlot::GetCurrentContext() == nullptr)
        ImPlot::CreateContext();
#endif
}

TCodePlayer::~TCodePlayer()
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
        loadPath = path;
    }
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
    
    SDL_atomic_t scriptTimeMs = { 0 };
    
    volatile float speed = 1.f;

    TCodePlayer* player = nullptr;
    TCodeChannels* channel = nullptr;
    TCodeProducer* producer = nullptr;
} Thread;

void TCodePlayer::DrawWindow(bool* open, float currentTimeMs) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);

    ImGui::Begin("T-Code", open, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo("Port", &current_port, [](void* data, int idx, const char** out_text) -> bool {
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
        if (ImGui::Button("Open port", ImVec2(-1.f, 0.f))) {
            openPort(port_list[current_port]);
        }
    }
    if (port != nullptr && ImGui::Button("Close port", ImVec2(-1.f, 0.f))) {
        sp_close(port);
        sp_free_port(port);
        port = nullptr;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (ImGui::CollapsingHeader("Limits##ChannelLimits"))
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
                "Min: %d", "Max: %d", ImGuiSliderFlags_AlwaysClamp);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(0.3f * availWidth);
            stbsp_snprintf(buf, sizeof(buf), "%-6s (%s)##%s_Enable", TCodeChannels::Aliases[static_cast<int32_t>(chan)][2], c.Id, c.Id);
            ImGui::Checkbox(buf, &c.Enabled);
        };

        ImGui::TextUnformatted("Linear limits");
        limitsGui(TChannel::L0);
        limitsGui(TChannel::L1);
        limitsGui(TChannel::L2);
        limitsGui(TChannel::L3);

        ImGui::Separator();

        ImGui::TextUnformatted("Rotation limits");
        limitsGui(TChannel::R0);
        limitsGui(TChannel::R1);
        limitsGui(TChannel::R2);

        ImGui::Separator();

        ImGui::TextUnformatted("Vibration limits");
        limitsGui(TChannel::V0);
        limitsGui(TChannel::V1);
        limitsGui(TChannel::V2);

        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Global settings"))
    {
        ImGui::InputInt("Delay", &delay, 10, 10); OFS::Tooltip("Negative: Backward in time.\nPositive: Forward in time.");
        ImGui::SliderInt("Tickrate (Hz)", &tickrate, 60, 300, "%d", ImGuiSliderFlags_AlwaysClamp); 
        ImGui::Checkbox("Spline", &TCodeChannel::SplineMode);
        OFS::Tooltip("Smooth motion instead of linear.");
        ImGui::SameLine(); ImGui::Checkbox("Remap", &TCodeChannel::RemapToFullRange);
        OFS::Tooltip("Remap script to use the full range.\ni.e. scripts using the range 10 to 90 become 0 to 100");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextUnformatted("Outputs");
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    OFS::Tooltip("You can right click sliders.");

    for (int i = 0; i < tcode.channels.size(); i++) {
        if(i == 4 || i == 7) ImGui::Spacing();
        auto& c = tcode.channels[i];
        if (!c.Enabled) continue;
        auto& p = prod.producers[i];
        ImGui::PushID(i);
        if (OFS::BoundedSliderInt(c.Id, &c.NextTCodeValue, TCodeChannel::MinChannelValue, TCodeChannel::MaxChannelValue, c.limits[0], c.limits[1], "%d", ImGuiSliderFlags_AlwaysClamp));
        if (ImGui::BeginPopupContextItem())
        {
            ImGui::MenuItem("Invert", NULL, &c.Invert);
            ImGui::MenuItem("Rebalance", NULL, &c.Rebalance); OFS::Tooltip("Balance around 500 even with unevenly spread limits.");
            ImGui::Separator();
            auto activeIdx = prod.GetProd(static_cast<TChannel>(i)).ScriptIdx();
            
            for (int32_t scriptIdx = 0; scriptIdx < prod.LoadedScripts.size(); scriptIdx++) {
                if (auto script = prod.LoadedScripts[scriptIdx].lock()) {
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
        int32_t ms = std::round(currentTimeMs);
        prod.sync(ms, 1.f);
        prod.tick(ms, 1.f);
        const char* cmd = tcode.GetCommandSpeed(500);
        if (cmd != nullptr && port != nullptr) {
            int len = strlen(cmd);
            if (sp_blocking_write(port, cmd, len, 0) < SP_OK) {
                LOG_ERROR("Failed to write to serial port.");
            }
        }
    }

#ifndef NDEBUG
    static bool debugShowPlot = false;
    ImGui::Checkbox("Plot", &debugShowPlot);
#endif
	ImGui::End();


#ifndef NDEBUG
    if (!debugShowPlot) return;
    ImGui::Begin("Plot");

    static float history = 10.0f;
    static float t = (SDL_GetTicks()/1000.f) + 0.1f;
    t += ImGui::GetIO().DeltaTime;

    static ScrollingBuffer RawSpeed;
    static ScrollingBuffer RawData;

    static ScrollingBuffer FilteredSpeed;
    static ScrollingBuffer FilteredData;

    float timestamp = SDL_GetTicks() / 1000.f;
    FilteredData.AddPoint(timestamp,  prod.GetProd(TChannel::L0).LastValue);
    RawData.AddPoint(timestamp, prod.GetProd(TChannel::L0).LastValueRaw);
    FilteredSpeed.AddPoint(timestamp, prod.GetProd(TChannel::L0).FilteredSpeed);
    RawSpeed.AddPoint(timestamp, prod.GetProd(TChannel::L0).RawSpeed);

    static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;
    ImPlot::SetNextPlotLimitsX(t - history, t, ImGuiCond_Always);
    if (ImPlot::BeginPlot("##EuroComp", NULL, NULL, ImVec2(-1, -1), 0, rt_axis, rt_axis | ImPlotAxisFlags_LockMin)) {
        ImPlot::PlotLine("RawSpeed", &RawSpeed.Data[0].x, &RawSpeed.Data[0].y, RawSpeed.Data.size(), RawSpeed.Offset, 2 * sizeof(float));
        ImPlot::PlotLine("FilteredSpeed", &FilteredSpeed.Data[0].x, &FilteredSpeed.Data[0].y, FilteredSpeed.Data.size(), FilteredSpeed.Offset, 2 * sizeof(float));

        ImPlot::PlotLine("Raw", &RawData.Data[0].x, &RawData.Data[0].y, RawData.Data.size(), RawData.Offset, 2 * sizeof(float));
        ImPlot::PlotLine("Filtered", &FilteredData.Data[0].x, &FilteredData.Data[0].y, FilteredData.Data.size(), FilteredData.Offset, 2 * sizeof(float));
        ImPlot::EndPlot();
    }

    ImGui::End();
#endif
}



static int32_t TCodeThread(void* threadData) noexcept {
    TCodeThreadData* data = (TCodeThreadData*)threadData;

    LOG_INFO("T-Code thread started...");
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int scriptTimeMs = 0;

    data->producer->sync(SDL_AtomicGet(&data->scriptTimeMs), data->player->tickrate);

    while (!data->requestStop) {
        float tickrate = data->player->tickrate;
        float tickDurationSeconds = 1.f / tickrate;
        auto currentTime = std::chrono::high_resolution_clock::now();

        int32_t delay = data->player->delay;
        int32_t data_scriptTimeMs = SDL_AtomicGet(&data->scriptTimeMs);
        
        std::chrono::duration<float> duration = currentTime - startTime;

        int32_t currentTimeMs = (((duration.count()*1000.f) * data->speed) + scriptTimeMs) - delay;
        int32_t syncTimeMs =  data_scriptTimeMs - delay;
        if (std::abs(currentTimeMs - syncTimeMs) >= 60) {
            LOGF_INFO("Resync -> %d", currentTimeMs - syncTimeMs);
            LOGF_INFO("prev: %d new: %d", currentTimeMs, syncTimeMs);

            scriptTimeMs = data_scriptTimeMs;
            startTime = std::move(currentTime);
            currentTimeMs = syncTimeMs;

            data->producer->sync(currentTimeMs, tickrate);
        }
        else {
            // tick producers
            data->producer->tick(currentTimeMs, tickrate);
        }
        
        // update channels
        const char* cmd = data->channel->GetCommand();

        if (cmd != nullptr && data->player->port != nullptr) {
            int len = strlen(cmd);
            if (sp_blocking_write(data->player->port, cmd, len, 0) < SP_OK) {
                LOG_ERROR("Failed to write to serial port.");
            }
        }

        while((duration = std::chrono::high_resolution_clock::now() - currentTime).count() < tickDurationSeconds) {
            /* Spin wait */
            OFS_PAUSE_INTRIN();
            int ms = (duration.count() / 0.001f) - 1;
            if (ms > 0) { SDL_Delay(ms); }
        }
    } 
    data->running = false;
    data->requestStop = false;
    LOG_INFO("T-Code thread stopped.");

    return 0;
}

void TCodePlayer::setScripts(std::vector<std::weak_ptr<const Funscript>>&& scripts) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    prod.LoadedScripts = std::move(scripts);
        
    // assume first is always stroke
    if (prod.GetProd(TChannel::L0).ScriptIdx() == -1) {
        prod.GetProd(TChannel::L0).SetScript(0);
    }

    for(int scriptIndex = 0; scriptIndex < prod.LoadedScripts.size(); scriptIndex++)
    {
        auto& script = prod.LoadedScripts[scriptIndex];
        if (auto locked = script.lock())
        {
            for (int i=0; i < static_cast<int>(TChannel::TotalCount); i++)
            {
                if (prod.GetProd(static_cast<TChannel>(i)).ScriptIdx() >= 0 // skip all which are already set
                    || prod.GetProd(static_cast<TChannel>(i)).ScriptIdx() == -2) { continue; }  // -2 is deliberatly unset
                auto& aliases = TCodeChannels::Aliases[i];
                for (auto& alias : aliases)
                {
                    if (Util::StringEndsWith(locked->Title, alias)) {
                        prod.GetProd(static_cast<TChannel>(i)).SetScript(scriptIndex);
                        break;
                    }
                }
            }
        }
    }
    prod.SetChannels(&tcode);
}

void TCodePlayer::play(float currentTimeMs, std::vector<std::weak_ptr<const Funscript>>&& scripts) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Thread.running) {
        Thread.running = true;
        Thread.player = this;
        Thread.channel = &this->tcode;
        SDL_AtomicSet(&Thread.scriptTimeMs, std::round(currentTimeMs));
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

void TCodePlayer::sync(float currentTimeMs, float speed) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    Thread.speed = speed;
    SDL_AtomicSet(&Thread.scriptTimeMs, std::round(currentTimeMs));
}

void TCodePlayer::reset() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (Thread.running) {
        Thread.requestStop = true;
        while (!Thread.running) { SDL_Delay(1); }
    }
    prod.ClearChannels();
}
