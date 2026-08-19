#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/fileio.h"
#include "../../common/display.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "core.h"
#include "../input/core_input_meta.h"
#include "../input/hotkeys.h"
#include "../cheevo/cheevo.h"
#include "../netplay/netplay.h"
#include "muxretro.h"
#include "subsystem.h"
#include "../ui/options.h"
#include "paths.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../state/vfs.h"
#include "../video/hw_render.h"

static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

#define MUX_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB 0x800003

static bool clear_all_thread_waits(const unsigned clear, void *data) {
    (void) clear;
    (void) data;

    return true;
}

static struct retro_disk_control_callback disk_control_storage;
static struct retro_disk_control_ext_callback disk_control_ext_storage;

static const struct retro_disk_control_callback *disk_control_cb = NULL;
static const struct retro_disk_control_ext_callback *disk_control_ext_cb = NULL;

static int core_wants_hw_render = 0;
static int hw_render_refused = 0;

int environment_core_wants_hw_render(void) {
    return core_wants_hw_render;
}

int environment_hw_render_refused(void) {
    return hw_render_refused;
}

static struct retro_system_av_info pending_av_info;
static int av_info_pending = 0;

static retro_frame_time_callback_t frame_time_cb = NULL;
static retro_usec_t frame_time_reference = 0;
static uint64_t frame_time_last_counter = 0;
static int frame_time_last_valid = 0;
static uint32_t frame_time_clamp_count = 0;
static retro_usec_t frame_time_clamp_peak = 0;

#define FRAME_TIME_MAX_REFERENCE_MULTIPLIER 4

enum retro_pixel_format mux_retro_get_pixel_format(void) {
    return pixel_format;
}

int mux_retro_disk_get_num_images(void) {
    int count = 0;
    if (disk_control_ext_cb && disk_control_ext_cb->get_num_images)
        count = (int) disk_control_ext_cb->get_num_images();
    else if (disk_control_cb && disk_control_cb->get_num_images)
        count = (int) disk_control_cb->get_num_images();

    core_cache_disc_count(count);
    return count;
}

unsigned mux_retro_disk_get_image_index(void) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_image_index) return disk_control_ext_cb->get_image_index();
    if (disk_control_cb && disk_control_cb->get_image_index) return disk_control_cb->get_image_index();
    return 0;
}

bool mux_retro_disk_set_image_index(const unsigned index) {
    if (disk_control_ext_cb && disk_control_ext_cb->set_image_index) return disk_control_ext_cb->set_image_index(index);
    if (disk_control_cb && disk_control_cb->set_image_index) return disk_control_cb->set_image_index(index);
    return false;
}

bool mux_retro_disk_set_eject_state(const bool ejected) {
    if (disk_control_ext_cb && disk_control_ext_cb->set_eject_state) {
        return disk_control_ext_cb->set_eject_state(ejected);
    }
    if (disk_control_cb && disk_control_cb->set_eject_state) return disk_control_cb->set_eject_state(ejected);
    return false;
}

bool mux_retro_disk_get_eject_state(void) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_eject_state) return disk_control_ext_cb->get_eject_state();
    if (disk_control_cb && disk_control_cb->get_eject_state) return disk_control_cb->get_eject_state();
    return false;
}

bool mux_retro_disk_get_image_label(const unsigned index, char *label, const size_t len) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_image_label) {
        return disk_control_ext_cb->get_image_label(index, label, len);
    }
    return false;
}

static void mux_retro_log_printf(const enum retro_log_level level, const char *fmt, ...) {
    char msg[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    size_t msg_len = strlen(msg);
    while (msg_len > 0 && (msg[msg_len - 1] == '\n' || msg[msg_len - 1] == '\r'))
        msg[--msg_len] = '\0';

    switch (level) {
        case RETRO_LOG_DEBUG:
            LOG_DEBUG(mux_module, "core: %s", msg);
            break;
        case RETRO_LOG_ERROR:
            LOG_ERROR(mux_module, "core: %s", msg);
            break;
        case RETRO_LOG_WARN:
            LOG_WARN(mux_module, "core: %s", msg);
            break;
        default:
            LOG_INFO(mux_module, "core: %s", msg);
            break;
    }
}

bool mux_retro_environment_cb(const unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            *(bool *) data = true;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX: {
            return data && netplay_get_client_index(data);
        }

        case RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE: {
            netplay_set_netpacket_interface(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            pixel_format = *(const enum retro_pixel_format *) data;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            static const char *dir = STORAGE_BIOS;
            *(const char **) data = dir;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            static char dir[PATH_MAX] = "";
            if (!dir[0]) {
                char core_name[MAX_BUFFER_SIZE];
                core_get_name(core_file_path, core_name, sizeof(core_name));
                snprintf(dir, sizeof(dir), "%s/%s", RETRO_SRM_PATH, core_name);
                create_directories(dir, 0);
            }
            *(const char **) data = dir;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            static struct retro_log_callback log_cb;
            log_cb.log = mux_retro_log_printf;
            *(struct retro_log_callback *) data = log_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: {
            subsystem_store(data);
            subsystem_log_resolved();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            if (options_count == 0) options_store_legacy(data);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *v = data;
            if (v) v->value = v->key ? options_get_value(v->key) : NULL;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLE: {
            const struct retro_variable *v = data;
            if (!v) return true;
            if (!v->key || !*v->key || !v->value || !*v->value) return false;
            return options_set(v->key, v->value);
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            *(bool *) data = options_dirty;
            options_dirty = false;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
            *(unsigned *) data = 2;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            options_store_v1(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            const struct retro_core_options_intl *intl = data;
            if (intl && intl->us) options_store_v1(intl->us);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
            options_store_v2(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
            const struct retro_core_options_v2_intl *intl = data;
            if (intl && intl->us) options_store_v2(intl->us);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
            return true;

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: {
            if (!data) {
                disk_control_cb = NULL;
                return true;
            }

            disk_control_storage = *(const struct retro_disk_control_callback *) data;
            disk_control_cb = &disk_control_storage;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: {
            *(unsigned *) data = 1;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
            if (!data) {
                disk_control_ext_cb = NULL;
                return true;
            }

            disk_control_ext_storage = *(const struct retro_disk_control_ext_callback *) data;
            disk_control_ext_cb = &disk_control_ext_storage;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const struct retro_message *msg = data;
            if (msg && msg->msg && ui_pnl_message)
                pause_menu_show_toast_timed(msg->msg, msg->frames > 0 ? (uint32_t) msg->frames * 16 : 3000);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
            const struct retro_message_ext *msg = data;
            if (msg && msg->msg && ui_pnl_message)
                pause_menu_show_toast_timed(msg->msg, msg->duration > 0 ? (uint32_t) msg->duration : 3000);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
            if (data) {
                int flags = 0;
                if (!video_bridge_get_frame_skip()) flags |= RETRO_AV_ENABLE_VIDEO;
                if (!audio_bridge_is_muted()) flags |= RETRO_AV_ENABLE_AUDIO;
                *(int *) data = flags;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            return vfs_bridge_get_interface(data);
        }

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            return rumble_bridge_get_interface(data);
        }

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: {
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
            core_input_meta_set_controller_info(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
            core_input_meta_set_input_descriptors(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
            cheevo_set_memory_map(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS: {
            cheevo_set_core_support(data && *(const bool *) data);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
            if (data) *(uint64_t *) data = UINT64_C(1) << RETRO_DEVICE_JOYPAD | UINT64_C(1) << RETRO_DEVICE_ANALOG;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: {
            if (data) *(unsigned *) data = 1 + MUX_INPUT_MAX_EXTRA_PLAYERS;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
            const struct retro_audio_buffer_status_callback *cb = data;
            audio_bridge_set_buffer_status_callback(cb ? cb->callback : NULL);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: {
            const unsigned *ms = data;
            audio_bridge_request_min_latency(ms ? *ms : 0);
            audio_bridge_apply_pending_min_latency();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_ROTATION: {
            const unsigned *rot = data;
            if (!rot) return false;
            video_bridge_set_core_rotation((int) *rot);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            const struct retro_game_geometry *geom = data;
            if (!geom) return true;
            video_bridge_set_geometry(geom->base_width, geom->base_height, geom->aspect_ratio);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            const struct retro_system_av_info *info = data;
            if (!info) return false;
            pending_av_info = *info;
            av_info_pending = 1;

            environment_apply_pending_av_info();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
            const struct retro_frame_time_callback *cb = data;
            frame_time_cb = cb ? cb->callback : NULL;
            frame_time_reference = cb ? cb->reference : 0;
            frame_time_last_valid = 0;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
            if (data) *(bool *) data = hotkeys_is_fast_forward_active();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: {
            if (data) {
                const int panel_hz = display_panel_refresh_hz();
                *(float *) data = panel_hz > 0 ? (float) panel_hz : frame_pacer_get_refresh_hz();
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: {
            if (data) *(unsigned *) data = audio_bridge_target_sample_rate();
            return true;
        }

        case MUX_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB: {
            if (!data) return false;

            *(retro_environment_t *) data = clear_all_thread_waits;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
            struct retro_throttle_state *throttle = data;
            if (throttle) {
                double run_rate = core_get_target_fps();
                const double pace_ms = core_auto_pace_target_ms();
                if (pace_ms > 0.0) run_rate = 1000.0 / pace_ms;

                if (hotkeys_is_fast_forward_active()) {
                    throttle->mode = RETRO_THROTTLE_FAST_FORWARD;
                    throttle->rate = 0.0f;
                } else if (hotkeys_is_slow_motion_active()) {
                    throttle->mode = RETRO_THROTTLE_SLOW_MOTION;
                    throttle->rate =
                        (float) (run_rate * session_settings_slowmo_speed_value(session_settings.slowmo_speed));
                } else if (session_settings.fps_limit == fps_limit_auto) {
                    const int panel_hz = display_panel_refresh_hz();
                    throttle->mode = core_content_needs_pacing() || (panel_hz > 0 && run_rate > (double) panel_hz)
                                         ? RETRO_THROTTLE_VSYNC
                                         : RETRO_THROTTLE_NONE;
                    throttle->rate = (float) run_rate;
                } else if (session_settings.fps_limit == fps_limit_none) {
                    throttle->mode = RETRO_THROTTLE_UNBLOCKED;
                    throttle->rate = 0.0f;
                } else {
                    throttle->mode = RETRO_THROTTLE_NONE;
                    throttle->rate = 50.0f;
                }
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SHUTDOWN: {
            hotkeys_request_quit();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_HW_RENDER: {
            core_wants_hw_render = 1;

            if (session_settings.game_renderer == game_renderer_software) {
                hw_render_refused = 1;
                LOG_INFO(mux_module, "hw_render: refused, Game Renderer is set to Software");
                return false;
            }

            return hw_render_bridge_negotiate(data);
        }

        default:
            return false;
    }
}

int environment_av_info_pending(void) {
    return av_info_pending;
}

void environment_apply_pending_av_info(void) {
    if (!av_info_pending) return;
    av_info_pending = 0;
    core_cache_system_av_info(&pending_av_info);

    if (hw_render_bridge_active()) {
        hw_render_bridge_configure(pending_av_info.geometry.max_width, pending_av_info.geometry.max_height);
    }

    video_bridge_set_geometry(
        pending_av_info.geometry.base_width, pending_av_info.geometry.base_height, pending_av_info.geometry.aspect_ratio
    );

    audio_bridge_reset_content_rate();
    if (pending_av_info.timing.fps > 0.0) core_set_target_fps(pending_av_info.timing.fps);
    if (pending_av_info.timing.sample_rate > 0.0) audio_bridge_reconfigure_rate(pending_av_info.timing.sample_rate);

    LOG_INFO(
        mux_module, "Core AV info applied: %.4f Hz, %.0f Hz audio, %ux%u (max %ux%u)", pending_av_info.timing.fps,
        pending_av_info.timing.sample_rate, pending_av_info.geometry.base_width, pending_av_info.geometry.base_height,
        pending_av_info.geometry.max_width, pending_av_info.geometry.max_height
    );
}

void environment_notify_frame_time(void) {
    if (!frame_time_cb) return;

    const int throttled = hotkeys_is_fast_forward_active() || hotkeys_is_slow_motion_active() || netplay_is_active();
    retro_usec_t usec = frame_time_reference;

    if (!throttled) {
        const uint64_t now = SDL_GetPerformanceCounter();
        if (frame_time_last_valid) {
            const double ns = (double) (now - frame_time_last_counter) * 1e9 / (double) SDL_GetPerformanceFrequency();
            usec = (retro_usec_t) (ns / 1000.0);

            if (frame_time_reference > 0 && usec > frame_time_reference * FRAME_TIME_MAX_REFERENCE_MULTIPLIER) {
                if (usec > frame_time_clamp_peak) frame_time_clamp_peak = usec;
                frame_time_clamp_count++;
                usec = frame_time_reference;
            }
        }
        frame_time_last_counter = now;
        frame_time_last_valid = 1;
    } else {
        frame_time_last_valid = 0;
    }

    frame_time_cb(usec);
}

int environment_frame_time_callback_active(void) {
    return frame_time_cb != NULL;
}

uint32_t environment_frame_time_clamp_count(void) {
    return frame_time_clamp_count;
}

double environment_frame_time_clamp_peak_ms(void) {
    return (double) frame_time_clamp_peak / 1000.0;
}
