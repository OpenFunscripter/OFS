#define OFS_MPV_LOADER_NO_MACROS
#include "OFS_MpvLoader.h"

#ifndef WIN32
#include "SDL_loadso.h"

static void* mpvHandle = nullptr;

mpv_create_FUNC MpvLoader::mpv_create_REAL = NULL;
mpv_wait_event_FUNC MpvLoader::mpv_wait_event_REAL  = NULL;
mpv_observe_property_FUNC MpvLoader::mpv_observe_property_REAL  = NULL;
mpv_render_context_update_FUNC MpvLoader::mpv_render_context_update_REAL  = NULL;
mpv_render_context_render_FUNC MpvLoader::mpv_render_context_render_REAL = NULL;
mpv_set_option_string_FUNC MpvLoader::mpv_set_option_string_REAL = NULL;
mpv_set_property_string_FUNC MpvLoader::mpv_set_property_string_REAL = NULL;
mpv_request_log_messages_FUNC MpvLoader::mpv_request_log_messages_REAL = NULL;
mpv_command_async_FUNC MpvLoader::mpv_command_async_REAL = NULL;
mpv_render_context_create_FUNC MpvLoader::mpv_render_context_create_REAL = NULL;
mpv_initialize_FUNC MpvLoader::mpv_initialize_REAL = NULL;
mpv_set_wakeup_callback_FUNC MpvLoader::mpv_set_wakeup_callback_REAL = NULL;
mpv_render_context_set_update_callback_FUNC MpvLoader::mpv_render_context_set_update_callback_REAL = NULL;
mpv_set_property_async_FUNC MpvLoader::mpv_set_property_async_REAL = NULL;
mpv_render_context_free_FUNC MpvLoader::mpv_render_context_free_REAL = NULL;
mpv_detach_destroy_FUNC MpvLoader::mpv_detach_destroy_REAL = NULL;

#define LOAD_FUNCTION(name) name##_REAL = (name##_FUNC)SDL_LoadFunction(mpvHandle, #name); if(!name##_REAL) return false

bool MpvLoader::Load() noexcept
{
    if(mpvHandle) return true;
    /// lib64/libmpv.so.1
    mpvHandle = SDL_LoadObject("libmpv.so.1");
    LOAD_FUNCTION(mpv_create);
    LOAD_FUNCTION(mpv_wait_event);
    LOAD_FUNCTION(mpv_observe_property);
    LOAD_FUNCTION(mpv_render_context_update);
    LOAD_FUNCTION(mpv_render_context_render);
    LOAD_FUNCTION(mpv_set_option_string);
    LOAD_FUNCTION(mpv_set_property_string);
    LOAD_FUNCTION(mpv_request_log_messages);
    LOAD_FUNCTION(mpv_command_async);
    LOAD_FUNCTION(mpv_render_context_create);
    LOAD_FUNCTION(mpv_initialize);
    LOAD_FUNCTION(mpv_set_wakeup_callback);
    LOAD_FUNCTION(mpv_render_context_set_update_callback);
    LOAD_FUNCTION(mpv_set_property_async);
    LOAD_FUNCTION(mpv_render_context_free);
    LOAD_FUNCTION(mpv_detach_destroy);
    return true;
}

void MpvLoader::Unload() noexcept
{
    if(!mpvHandle) return;
    SDL_UnloadObject(mpvHandle);
}

#endif