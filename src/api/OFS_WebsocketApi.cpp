#include "OFS_WebsocketApi.h"
#include "OFS_WebsocketApiClient.h"
#include "OFS_FileLogging.h"
#include "OFS_EventSystem.h"

#include "OpenFunscripter.h"
#include "OFS_VideoplayerEvents.h"
#include "OFS_WebsocketApiEvents.h"
#include "state/WebsocketApiState.h"
#include "state/states/ChapterState.h"

#include "civetweb.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"

struct CivetwebContext
{
    mg_context* web = nullptr;
	char errtxtbuf[256] = {0};
	SDL_atomic_t clientsConnected = {0};
};

#define CTX static_cast<CivetwebContext*>(ctx)

static const char WS_URL[] = "/ofs";

/* Define websocket sub-protocols. */
/* This must be static data, available between mg_start and mg_stop. */
static const char* subprotocols[] = {"ofs-api.json", NULL};
static struct mg_websocket_subprotocols wsprot = {1, subprotocols};

/* Handler for new websocket connections. */
static int ws_connect_handler(const struct mg_connection *conn, void *ctx) noexcept
{
	/* Allocate data for websocket client context, and initialize context. */
    auto clientCtx = new OFS_WebsocketClient();
	if (!clientCtx) {
		/* reject client */
		return 1;
	}

	mg_set_user_connection_data(conn, clientCtx);

	/* DEBUG: New client connected (but not ready to receive data yet). */
	const struct mg_request_info *ri = mg_get_request_info(conn);
	LOGF_INFO("Client connected with subprotocol: %s\n",
	       ri->acceptedWebSocketSubprotocol);

	SDL_AtomicIncRef(&CTX->clientsConnected);
	return 0;
}

/* Handler indicating the client is ready to receive data. */
static void ws_ready_handler(struct mg_connection *conn, void *user_data) noexcept
{
	(void)user_data; /* unused */

	/* Get websocket client context information. */
	auto clientCtx = (OFS_WebsocketClient*)mg_get_user_connection_data(conn);
	clientCtx->InitializeConnection(conn);

	// const struct mg_request_info *ri = mg_get_request_info(conn);

	/* DEBUG: New client ready to receive data. */
	LOG_INFO("Client ready to receive data\n");
}


/* Handler indicating the client sent data to the server. */
static int ws_data_handler(struct mg_connection *conn,
                int opcode,
                char *data,
                size_t datasize,
                void *user_data) noexcept
{
	(void)user_data; /* unused */

	/* Get websocket client context information. */
	auto clientCtx = (OFS_WebsocketClient*)mg_get_user_connection_data(conn);
	// const struct mg_request_info *ri = mg_get_request_info(conn);

	const char *messageType = "";
	switch (opcode & 0xf) {
	case MG_WEBSOCKET_OPCODE_TEXT:
		messageType = "text";
		clientCtx->ReceiveText(data, datasize);
		break;
	case MG_WEBSOCKET_OPCODE_BINARY:
		messageType = "binary";
		break;
	case MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE:
		messageType = "conn_close";
		break;
	case MG_WEBSOCKET_OPCODE_PING:
		messageType = "ping";
		break;
	case MG_WEBSOCKET_OPCODE_PONG:
		messageType = "pong";
		break;
	}
	LOGF_DEBUG("Websocket received %lu bytes of %s data from client\n",
	       (unsigned long)datasize,
	       messageType);
	return 1;
}


/* Handler indicating the connection to the client is closing. */
static void ws_close_handler(const struct mg_connection *conn, void *ctx) noexcept
{
	/* Get websocket client context information. */
	auto clientCtx = (OFS_WebsocketClient*)mg_get_user_connection_data(conn);

	/* DEBUG: Client has left. */
	LOG_INFO("Client closing connection\n");
	SDL_AtomicDecRef(&CTX->clientsConnected);

	/* Free memory allocated for client context in ws_connect_handler() call. */
    delete clientCtx;
}

static int EventSerializationThread(void* user) noexcept
{
	auto ctx = static_cast<EventSerializationContext*>(user);
	auto waitMut = SDL_CreateMutex();
	SDL_LockMutex(waitMut);
	while(!ctx->shouldExit)
	{
		if(SDL_CondWait(ctx->processCond, waitMut) == 0)
		{
			SDL_AtomicLock(&ctx->eventLock);
			for(auto& ev : ctx->events)
			{
				auto toJson = dynamic_cast<ToJsonInterface*>(ev.get());
				nlohmann::json json;
				toJson->Serialize(json);
				auto jsonText = Util::SerializeJson(json);
				EV::Queue().directDispatch(WsSerializedEvent::EventType, 
					std::move(EV::Make<WsSerializedEvent>(std::move(jsonText))));
			}
			ctx->events.clear();
			SDL_AtomicUnlock(&ctx->eventLock);
		}
	}
	SDL_DestroyMutex(waitMut);
	ctx->hasExited = true;
	return 0;
}

OFS_WebsocketApi::OFS_WebsocketApi() noexcept
{
	stateHandle = OFS_AppState<WebsocketApiState>::Register(WebsocketApiState::StateName);
	eventSerializationCtx = std::make_unique<EventSerializationContext>();

	auto serializationThread = SDL_CreateThread(
		EventSerializationThread, "WebsocketEventSerialization", eventSerializationCtx.get());
	SDL_DetachThread(serializationThread);

	EV::Queue().appendListener(VideoLoadedEvent::EventType, VideoLoadedEvent::HandleEvent(
		[this](const VideoLoadedEvent* ev) noexcept
		{
			if(ClientsConnected() > 0 && ev->playerType == VideoplayerType::Main)
			{
				eventSerializationCtx->Push<WsMediaChange>(ev->videoPath);
			}
		}
	));

	EV::Queue().appendListener(DurationChangeEvent::EventType, DurationChangeEvent::HandleEvent(
		[this](const DurationChangeEvent* ev) noexcept
		{
			if(ClientsConnected() > 0 && ev->playerType == VideoplayerType::Main)
			{
				eventSerializationCtx->Push<WsDurationChange>(ev->duration);
			}
		}
	));

	EV::Queue().appendListener(PlaybackSpeedChangeEvent::EventType, PlaybackSpeedChangeEvent::HandleEvent(
		[this](const PlaybackSpeedChangeEvent* ev) noexcept
		{
			if(ClientsConnected() > 0 && ev->playerType == VideoplayerType::Main)
			{
				eventSerializationCtx->Push<WsPlaybackSpeedChange>(ev->playbackSpeed);
			}
		}
	));

	EV::Queue().appendListener(PlayPauseChangeEvent::EventType, PlayPauseChangeEvent::HandleEvent(
		[this](const PlayPauseChangeEvent* ev) noexcept
		{
			if(ClientsConnected() > 0 && ev->playerType == VideoplayerType::Main)
			{
				eventSerializationCtx->Push<WsPlayChange>(!ev->paused);
			}
		}
	));

	EV::Queue().appendListener(TimeChangeEvent::EventType, TimeChangeEvent::HandleEvent(
		[this](const TimeChangeEvent* ev) noexcept
		{
			if(ClientsConnected() > 0 && ev->playerType == VideoplayerType::Main)
			{
				eventSerializationCtx->Push<WsTimeChange>(ev->time);
			}
		}
	));

	EV::Queue().appendListener(ProjectLoadedEvent::EventType, ProjectLoadedEvent::HandleEvent(
		[this](const ProjectLoadedEvent* ev) noexcept
		{
			if(ClientsConnected() > 0) 
			{
				// WsProjectChange remains handled by each internal client 
				// this makes this event really expensive depending on the number of connected clients
				EV::Queue().directDispatch(WsProjectChange::EventType, EV::Make<WsProjectChange>());
			}
		}
	));

	EV::Queue().appendListener(FunscriptNameChangedEvent::EventType, FunscriptNameChangedEvent::HandleEvent(
		[this](const FunscriptNameChangedEvent* ev) noexcept
		{
			if(ClientsConnected() > 0)
			{
				// Funscript name changes are handled as the old name being removed and the new one added
				auto app = OpenFunscripter::ptr;
				auto& projectState = app->LoadedProject->State();
				eventSerializationCtx->Push<WsFunscriptRemove>(ev->oldName);
				eventSerializationCtx->Push<WsFunscriptChange>(ev->Script->Title(), ev->Script->Data(), projectState.metadata);
			}
		}
	));

	EV::Queue().appendListener(MetadataChanged::EventType, 
		MetadataChanged::HandleEvent(
			[this](const MetadataChanged* ev) noexcept
			{
				auto app = OpenFunscripter::ptr;				
				for(int i=0, size=app->LoadedFunscripts().size(); i < size; i += 1)
				{
					auto scriptIdx = i;
					if(scriptIdx + 1 > this->scriptUpdateCooldown.size()) {
						scriptUpdateCooldown.resize(scriptIdx + 1, 0);
					}
					scriptUpdateCooldown[scriptIdx] = SDL_GetTicks();
				}
			}
		));
	
	EV::Queue().appendListener(ChapterStateChanged::EventType, 
		ChapterStateChanged::HandleEvent(
			[this](const ChapterStateChanged* ev) noexcept
			{
				auto app = OpenFunscripter::ptr;				
				for(int i=0, size=app->LoadedFunscripts().size(); i < size; i += 1)
				{
					auto scriptIdx = i;
					if(scriptIdx + 1 > this->scriptUpdateCooldown.size()) {
						scriptUpdateCooldown.resize(scriptIdx + 1, 0);
					}
					scriptUpdateCooldown[scriptIdx] = SDL_GetTicks();
				}
			}
		));

	EV::Queue().appendListener(FunscriptRemovedEvent::EventType, FunscriptRemovedEvent::HandleEvent(
		[this](const FunscriptRemovedEvent* ev) noexcept
		{
			if(ClientsConnected() > 0)
			{
				eventSerializationCtx->Push<WsFunscriptRemove>(ev->name);
			}
		}
	));

	EV::Queue().appendListener(FunscriptActionsChangedEvent::EventType, FunscriptActionsChangedEvent::HandleEvent(
		[this](const FunscriptActionsChangedEvent* ev) noexcept
		{
			if(ClientsConnected() > 0)
			{
				auto app = OpenFunscripter::ptr;
				auto it = std::find_if(app->LoadedFunscripts().begin(), app->LoadedFunscripts().end(), 
					[Script = ev->Script](auto& script) noexcept { return script.get() == Script; });

				if(it != app->LoadedFunscripts().end())
				{
					auto scriptIdx = std::distance(app->LoadedFunscripts().begin(), it);
					if(scriptIdx + 1 > this->scriptUpdateCooldown.size()) {
						scriptUpdateCooldown.resize(scriptIdx + 1, 0);
					}
					scriptUpdateCooldown[scriptIdx] = SDL_GetTicks();
				}
			}
		}
	));
}

int OFS_WebsocketApi::ClientsConnected() const noexcept
{
	return SDL_AtomicGet(&CTX->clientsConnected);
}

bool OFS_WebsocketApi::Init() noexcept
{
    if(ctx) return true;
    ctx = new CivetwebContext();
    if(mg_init_library(0) != 0)
        return false;

	auto& state = WebsocketApiState::State(stateHandle);
	if(state.serverActive) StartServer();

    return true;
}

bool OFS_WebsocketApi::StartServer() noexcept
{
	if(CTX->web) return true;
	auto& state = WebsocketApiState::State(stateHandle);

	const char* options[] = {"listening_ports", state.port.c_str(), "num_threads", "4", NULL, NULL};

    /* Start the server using the advanced API. */
	struct mg_callbacks callbacks = {0};

	struct mg_init_data mg_start_init_data = {0};
	mg_start_init_data.callbacks = &callbacks;
	mg_start_init_data.user_data = ctx;
	mg_start_init_data.configuration_options = options;

	struct mg_error_data mg_start_error_data = {0};
	mg_start_error_data.text = CTX->errtxtbuf;
	mg_start_error_data.text_buffer_size = sizeof(CTX->errtxtbuf);

	CTX->web = mg_start2(&mg_start_init_data, &mg_start_error_data);
    if(!CTX->web)
        return false;

    /* Register the websocket callback functions. */
	mg_set_websocket_handler_with_subprotocols(CTX->web,
	                                           WS_URL,
	                                           &wsprot,
	                                           ws_connect_handler,
	                                           ws_ready_handler,
	                                           ws_data_handler,
	                                           ws_close_handler,
	                                           ctx);
	return true;
}

void OFS_WebsocketApi::StopServer() noexcept
{
	if(CTX->web)
	{
		mg_stop(CTX->web);
		CTX->web = nullptr;
	}
}

void OFS_WebsocketApi::Update() noexcept
{
	if(ClientsConnected() <= 0) return;

	for(int i=0, size=scriptUpdateCooldown.size(); i < size; i += 1)
	{
		auto& cd = scriptUpdateCooldown[i];
		if(cd == 0) continue;
		if(SDL_GetTicks() - cd >= 200)
		{
			auto app = OpenFunscripter::ptr;
			if(i >= 0 && i < app->LoadedFunscripts().size())
			{
				auto& projectState = app->LoadedProject->State();
				auto& script = app->LoadedFunscripts()[i];
				eventSerializationCtx->Push<WsFunscriptChange>(script->Title(), script->Data(), projectState.metadata);
				LOGF_DEBUG("[WsFunscriptChange]: ScriptIdx: %d", i);
			}
			cd = 0;
		}
	}

	if(!eventSerializationCtx->EventsEmpty())
	{
		eventSerializationCtx->StartProcessing();
	}
	
	OFS_WebsocketClient::CommandBuffer.ProcessCommands();
}

void OFS_WebsocketApi::Shutdown() noexcept
{
	eventSerializationCtx->Shutdown();
	StopServer();
    mg_exit_library();
    delete CTX;
    ctx = nullptr;
}

void OFS_WebsocketApi::ShowWindow(bool* open) noexcept
{
	if(!*open) return;
	auto& state = WebsocketApiState::State(stateHandle);

	ImGui::Begin(TR_ID("WebsocketApi", Tr::WEBSOCKET_API), open, ImGuiWindowFlags_AlwaysAutoResize);
	bool serverRunning = CTX->web != nullptr;
	if(ImGui::Checkbox(TR(SERVER_ACTIVE), &serverRunning))
	{
		if(serverRunning) StartServer();
		else StopServer();
		state.serverActive = serverRunning;
	}

	if(serverRunning)
	{
		mg_server_port ports;
		mg_get_server_ports(CTX->web, 1, &ports);
		
		ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "ws://0.0.0.0:%d%s", ports.port, WS_URL);
		auto clientCount = ClientsConnected();
		ImGui::Text("%s: %d", TR(CLIENT_COUNT), clientCount);
	}

	auto textChanged = ImGui::InputText(TR(PORT), &state.port, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CharsDecimal,
		[](ImGuiInputTextCallbackData* data)
		{
			if(data->EventChar >= '0' && data->EventChar <= '9')
				return 0;
			return 1;
		});
	if(textChanged)
	{
		int port = SDL_atoi(state.port.c_str());
		if(port < 0 || port > std::numeric_limits<uint16_t>::max())
		{
			state.port = "8080";
		}
	}

	ImGui::End();
}