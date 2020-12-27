#include "player/OFS_Tcode.h"
#include "OFS_Util.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "SDL.h"

bool TCodeChannel::easing = false;
int32_t TCodeChannel::limits[2] = { 100, 900 };

void TCodePlayer::openPort(const char* name) noexcept
{
    if (port != nullptr) {
        c_serial_free(port);
        port = nullptr;
    }

    if (c_serial_new(&port, NULL) < 0) {
        LOG_ERROR("ERROR: Unable to create new serial port\n");
        port = nullptr;
        status = -1;
        return;
    }

    /*
     * The port name is the device to open(/dev/ttyS0 on Linux,
     * COM1 on Windows)
     */
    if (c_serial_set_port_name(port, name) < 0) {
        LOG_ERROR("ERROR: can't set port name\n");
        return;
    }

    c_serial_set_baud_rate(port, CSERIAL_BAUD_115200);
    c_serial_set_data_bits(port, CSERIAL_BITS_8);
    c_serial_set_stop_bits(port, CSERIAL_STOP_BITS_1);
    c_serial_set_parity(port, CSERIAL_PARITY_NONE);
    c_serial_set_flow_control(port, CSERIAL_FLOW_NONE);

    LOGF_DEBUG("Baud rate is %d\n", c_serial_get_baud_rate(port));

    /*
    * We want to get all line flags when they change
    */
    c_serial_set_serial_line_change_flags(port, CSERIAL_LINE_FLAG_ALL);
    status = c_serial_open(port);
    if (status < 0) {
        LOG_ERROR("ERROR: Can't open serial port\n");
        return;
    }
}

void TCodePlayer::tick() noexcept
{
    if (status < 0) return;

    data_length = sizeof(data);
    status = c_serial_read_data(port, data, &data_length, &lines);
    if (status < 0) {
        LOG_ERROR("Failed to read from serial port.");
        return;
    }


    LOGF_DEBUG("Got %d bytes of data\n", data_length);
    
    for (int x = 0; x < data_length; x++) {
        LOGF_DEBUG("    0x%02X (ASCII: %c)\n", data[x], data[x]);
    }
    LOGF_DEBUG("Serial line state: CD: %d CTS: %d DSR: %d DTR: %d RTS: %d RI: %d\n",
        lines.cd,
        lines.cts,
        lines.dsr,
        lines.dtr,
        lines.rts,
        lines.ri);

    status = c_serial_write_data(port, data, &data_length);
    if (status < 0) {
        LOG_ERROR("Failed to write to serial port.");
        return;
    }
}

TCodePlayer::TCodePlayer()
{
	c_serial_set_global_log_function(
		[](const char* logger_name, const struct SL_LogLocation* location, const enum SL_LogLevel level, const char* log_string) {
			LOGF_INFO("%s: %s",logger_name, log_string);
		});
}

static struct TcodeThreadData {
    bool running = false;
    bool requestStop = false;
    int32_t scriptTimeMs = 0.f;
    int tickrate = 30;
    int32_t delay = 0;

    TCodePlayer* player = nullptr;

    TCodeChannel L0;
} Thread;

void TCodePlayer::DrawWindow(bool* open) noexcept
{
    if (!*open) return;

    ImGui::Begin("T-Code", open, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo("Port", &current_port, [](void* data, int idx, const char** out_text) -> bool {
        const char** port_list = (const char**)data;
        *out_text = port_list[idx];
        return true;
        }, port_list, port_count);
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (port_list != nullptr) {
            c_serial_free_serial_ports_list(port_list);
        }
        port_list = c_serial_get_serial_ports_list();
        int x = 0;
        LOG_DEBUG("Available serial ports:\n");
        while (port_list[x] != NULL) {
            LOGF_DEBUG("%s\n", port_list[x]);
            x++;
        }
        port_count = x;
    }

    if (port_list != nullptr
        && port_list[0] != nullptr
        && current_port < port_count
        && port_list[current_port] != nullptr) {
        if (ImGui::Button("Open port", ImVec2(-1.f, 0.f))) {
            openPort(port_list[current_port]);
        }
    }

    ImGui::SliderInt2("Limits", TCodeChannel::limits, 100, 900);
    ImGui::Checkbox("Easing", &TCodeChannel::easing);
    ImGui::SameLine();
    ImGui::InputInt("Delay", &Thread.delay, 10, 10);
    ImGui::SliderInt("Tickrate", &Thread.tickrate, 30, 300);

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::SliderInt("L0", &Thread.L0.lastTcodeVal, TCodeChannel::limits[0], TCodeChannel::limits[1]);
    ImGui::SameLine(); ImGui::Text(" -> %s", Thread.L0.buf);
    ImGui::PopItemFlag();

	ImGui::End();
}


static int32_t TcodeThread(void* threadData) noexcept {
    TcodeThreadData* data = (TcodeThreadData*)threadData;

    LOG_INFO("T-Code thread started...");

    int startTicks = SDL_GetTicks();
    
    int scriptTimeMs = -1;

    data->L0.Sync(data->scriptTimeMs - Thread.delay);

    while (!data->requestStop) {
        int maxTicks = std::round(1000.f / data->tickrate) - 1;
        if (data->scriptTimeMs != scriptTimeMs) {
            scriptTimeMs = data->scriptTimeMs;
            int startTicks = SDL_GetTicks();

            data->L0.Sync(scriptTimeMs);
        }

        int32_t ticks = SDL_GetTicks();
        int32_t currentTimeMs = (ticks - startTicks) + data->scriptTimeMs;

        const char* cmd = data->L0.getCommand(currentTimeMs - Thread.delay);
        if (cmd != nullptr && data->player->status >= 0) {
            //LOGF_DEBUG("tcode: %s", cmd);
            int len = strlen(cmd);
            data->player->status = c_serial_write_data(data->player->port, (void*)cmd, &len);
            if (data->player->status < 0) {
                LOG_ERROR("Failed to write to serial port.");
                break;
            }
        }

        int delay = maxTicks - (SDL_GetTicks() - ticks);
        if (delay > 0) {
            SDL_Delay(delay);
        }
    }
    data->running = false;
    data->requestStop = false;
    LOG_INFO("T-Code thread stopped.");

    return 0;
}

void TCodePlayer::play(float currentTimeMs, const std::vector<FunscriptAction>& actions) noexcept
{
    if (!Thread.running) {
        Thread.scriptTimeMs = std::round(currentTimeMs);
        Thread.running = true;
        Thread.L0.actions = actions;
        Thread.player = this;
        auto t = SDL_CreateThread(TcodeThread, "TCodePlayer", &Thread);
        SDL_DetachThread(t);
    }
}

void TCodePlayer::stop() noexcept
{
    if (Thread.running) {
        Thread.requestStop = true;
    }
}
