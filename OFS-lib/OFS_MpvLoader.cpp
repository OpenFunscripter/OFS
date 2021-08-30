#include "OFS_MpvLoader.h"
#include "OFS_FileLogging.h"

#include "SDL_loadso.h"
#include <type_traits>

static void* mpvHandle = nullptr;

mpv_create_FUNC OFS_MpvLoader::mpv_create_REAL = NULL;
mpv_wait_event_FUNC OFS_MpvLoader::mpv_wait_event_REAL  = NULL;
mpv_observe_property_FUNC OFS_MpvLoader::mpv_observe_property_REAL  = NULL;
mpv_render_context_update_FUNC OFS_MpvLoader::mpv_render_context_update_REAL  = NULL;
mpv_render_context_render_FUNC OFS_MpvLoader::mpv_render_context_render_REAL = NULL;
mpv_set_option_string_FUNC OFS_MpvLoader::mpv_set_option_string_REAL = NULL;
mpv_set_property_string_FUNC OFS_MpvLoader::mpv_set_property_string_REAL = NULL;
mpv_request_log_messages_FUNC OFS_MpvLoader::mpv_request_log_messages_REAL = NULL;
mpv_command_async_FUNC OFS_MpvLoader::mpv_command_async_REAL = NULL;
mpv_render_context_create_FUNC OFS_MpvLoader::mpv_render_context_create_REAL = NULL;
mpv_initialize_FUNC OFS_MpvLoader::mpv_initialize_REAL = NULL;
mpv_set_wakeup_callback_FUNC OFS_MpvLoader::mpv_set_wakeup_callback_REAL = NULL;
mpv_render_context_set_update_callback_FUNC OFS_MpvLoader::mpv_render_context_set_update_callback_REAL = NULL;
mpv_set_property_async_FUNC OFS_MpvLoader::mpv_set_property_async_REAL = NULL;
mpv_render_context_free_FUNC OFS_MpvLoader::mpv_render_context_free_REAL = NULL;
mpv_destroy_FUNC OFS_MpvLoader::mpv_destroy_REAL = NULL;
mpv_render_context_report_swap_FUNC OFS_MpvLoader::mpv_render_context_report_swap_REAL = NULL;

#define LOAD_FUNCTION(name) name##_REAL = (name##_FUNC)SDL_LoadFunction(mpvHandle, #name);\
if(!name##_REAL) {\
    LOGF_ERROR("Failed to load \"%s\"", #name);\
    LOG_ERROR(SDL_GetError());\
    return false;\
}\
static_assert(std::is_same<decltype(&name), name##_FUNC>::value, "Function pointer signature doesn't match libmpv function signature.")

bool OFS_MpvLoader::Load() noexcept
{
    if(mpvHandle) return true;
    const char* lib = nullptr;
    #if defined(WIN32)
    lib = "mpv-1.dll";
    #elif defined(__APPLE__)
    lib = "libmpv.dylib";
    #else // linux
    lib = "libmpv.so.1";
    #endif

    mpvHandle = SDL_LoadObject(lib);
    if(!mpvHandle) {
        LOGF_ERROR("Failed to load \"%s\"", lib);
        return false;
    }
    
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
    LOAD_FUNCTION(mpv_destroy);
    LOAD_FUNCTION(mpv_render_context_report_swap);

    return true;
}

#define SET_NULL(name) name##_REAL = nullptr

void OFS_MpvLoader::Unload() noexcept
{
    if(!mpvHandle) return;
    SDL_UnloadObject(mpvHandle);
    mpvHandle = nullptr;
    SET_NULL(mpv_create);
    SET_NULL(mpv_wait_event);
    SET_NULL(mpv_observe_property);
    SET_NULL(mpv_render_context_update);
    SET_NULL(mpv_render_context_render);
    SET_NULL(mpv_set_option_string);
    SET_NULL(mpv_set_property_string);
    SET_NULL(mpv_request_log_messages);
    SET_NULL(mpv_command_async);
    SET_NULL(mpv_render_context_create);
    SET_NULL(mpv_initialize);
    SET_NULL(mpv_set_wakeup_callback);
    SET_NULL(mpv_render_context_set_update_callback);
    SET_NULL(mpv_set_property_async);
    SET_NULL(mpv_render_context_free);
    SET_NULL(mpv_destroy);
    SET_NULL(mpv_render_context_report_swap);
}