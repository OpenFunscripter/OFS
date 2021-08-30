#pragma once

#ifndef WIN32
#include <mpv/client.h>
#include <mpv/render_gl.h>


typedef mpv_handle* (*mpv_create_FUNC)(void);
typedef int (*mpv_initialize_FUNC)(mpv_handle *ctx);
typedef mpv_event* (*mpv_wait_event_FUNC)(mpv_handle *ctx, double timeout);
typedef int (*mpv_observe_property_FUNC)(mpv_handle *mpv, uint64_t reply_userdata, const char *name, mpv_format format);
typedef uint64_t (*mpv_render_context_update_FUNC)(mpv_render_context *ctx);
typedef int (*mpv_render_context_render_FUNC)(mpv_render_context *ctx, mpv_render_param *params);
typedef int (*mpv_set_option_string_FUNC)(mpv_handle *ctx, const char *name, const char *data);
typedef int (*mpv_set_property_string_FUNC)(mpv_handle *ctx, const char *name, const char *data);
typedef int (*mpv_request_log_messages_FUNC)(mpv_handle *ctx, const char *min_level);
typedef int (*mpv_command_async_FUNC)(mpv_handle *ctx, uint64_t reply_userdata, const char **args);
typedef int (*mpv_render_context_create_FUNC)(mpv_render_context **res, mpv_handle *mpv, mpv_render_param *params);
typedef void (*mpv_set_wakeup_callback_FUNC)(mpv_handle *ctx, void (*cb)(void *d), void *d);
typedef void (*mpv_render_context_set_update_callback_FUNC)(mpv_render_context *ctx, mpv_render_update_fn callback, void *callback_ctx);
typedef int (*mpv_set_property_async_FUNC)(mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format, void *data);
typedef void (*mpv_render_context_free_FUNC)(mpv_render_context *ctx);
typedef void (*mpv_detach_destroy_FUNC)(mpv_handle *ctx);

struct MpvLoader
{
    static mpv_create_FUNC mpv_create_REAL;
    static mpv_initialize_FUNC mpv_initialize_REAL;
    static mpv_wait_event_FUNC mpv_wait_event_REAL;
    static mpv_observe_property_FUNC mpv_observe_property_REAL;
    static mpv_render_context_update_FUNC mpv_render_context_update_REAL;
    static mpv_render_context_render_FUNC mpv_render_context_render_REAL;
    static mpv_set_option_string_FUNC mpv_set_option_string_REAL;
    static mpv_set_property_string_FUNC mpv_set_property_string_REAL;
    static mpv_request_log_messages_FUNC mpv_request_log_messages_REAL;
    static mpv_command_async_FUNC mpv_command_async_REAL;
    static mpv_render_context_create_FUNC mpv_render_context_create_REAL;
    static mpv_set_wakeup_callback_FUNC mpv_set_wakeup_callback_REAL;
    static mpv_render_context_set_update_callback_FUNC mpv_render_context_set_update_callback_REAL;
    static mpv_set_property_async_FUNC mpv_set_property_async_REAL;
    static mpv_render_context_free_FUNC mpv_render_context_free_REAL;
    static mpv_detach_destroy_FUNC mpv_detach_destroy_REAL;

    static bool Load() noexcept;
    static void Unload() noexcept;
};

#ifndef OFS_MPV_LOADER_NO_MACROS
#define mpv_create() MpvLoader::mpv_create_REAL()
#define mpv_initialize(ctx) MpvLoader::mpv_initialize_REAL(ctx)
#define mpv_wait_event(ctx, timeout) MpvLoader::mpv_wait_event_REAL(ctx, timeout)
#define mpv_observe_property(handle, reply_userdata, name, format) MpvLoader::mpv_observe_property_REAL(handle, reply_userdata, name, format)
#define mpv_render_context_update(ctx) MpvLoader::mpv_render_context_update_REAL(ctx)
#define mpv_render_context_render(ctx, params) MpvLoader::mpv_render_context_render_REAL(ctx, params)
#define mpv_set_option_string(ctx, name, data) MpvLoader::mpv_set_option_string_REAL(ctx, name, data)
#define mpv_set_property_string(ctx, name, data) MpvLoader::mpv_set_property_string_REAL(ctx, name, data)
#define mpv_request_log_messages(ctx, min_level) MpvLoader::mpv_request_log_messages_REAL(ctx, min_level)
#define mpv_command_async(ctx, reply_userdata, args) MpvLoader::mpv_command_async_REAL(ctx, reply_userdata, args)
#define mpv_render_context_create(res, mpv, params) MpvLoader::mpv_render_context_create_REAL(res, mpv, params)
#define mpv_set_wakeup_callback(ctx, cb, d) MpvLoader::mpv_set_wakeup_callback_REAL(ctx, cb, d)
#define mpv_render_context_set_update_callback(ctx, callback, callback_ctx) MpvLoader::mpv_render_context_set_update_callback_REAL(ctx, callback, callback_ctx)
#define mpv_set_property_async(ctx, reply_userdata, name, format, data) MpvLoader::mpv_set_property_async_REAL(ctx, reply_userdata, name, format, data)
#define mpv_render_context_free(ctx) MpvLoader::mpv_render_context_free_REAL(ctx)
#define mpv_detach_destroy(ctx) MpvLoader::mpv_detach_destroy_REAL(ctx)
#endif // OFS_MPV_LOADER_NO_MACROS
#endif