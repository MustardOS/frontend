#include "session.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <module/muxshare.h>
#include <common/config/config.h>
#include "../core/paths.h"
#include "assets.h"
#include "settings.h"

typedef enum { session_i16, session_text } session_kind;

typedef struct {
    const char *key;
    size_t offset;
    session_kind kind;
} session_field;

#define SESSION_I16(member)  {#member, offsetof(wasabi_video_config, member), session_i16}
#define SESSION_TEXT(member) {#member, offsetof(wasabi_video_config, member), session_text}

static const session_field fields[] = {
    SESSION_I16(scaling_mode),
    SESSION_I16(rotation),
    SESSION_I16(mirrored),
    SESSION_I16(aspect_ratio),
    SESSION_I16(scale_multiplier),
    SESSION_I16(texture_filter),
    SESSION_I16(border_colour),
    SESSION_I16(vignette_shape),
    SESSION_I16(vignette_scaling),
    SESSION_I16(vignette_width),
    SESSION_I16(vignette_height),
    SESSION_I16(vignette_offset_x),
    SESSION_I16(vignette_offset_y),
    SESSION_I16(vignette_softness),
    SESSION_I16(vignette_strength),
    SESSION_I16(vignette_colour),
    SESSION_TEXT(colour_filter),
    SESSION_TEXT(shader),
    SESSION_TEXT(shader_params),
    SESSION_TEXT(equaliser),
    SESSION_TEXT(equaliser_profile),
    SESSION_I16(brightness),
    SESSION_I16(contrast),
    SESSION_I16(saturation),
    SESSION_I16(hue_shift),
    SESSION_I16(gamma),
    SESSION_I16(visualiser),
    SESSION_I16(overlay_mode),
    SESSION_I16(overlay_pattern),
    SESSION_TEXT(overlay_image),
    SESSION_I16(overlay_opacity),
    SESSION_I16(overlay_x),
    SESSION_I16(overlay_y),
    SESSION_I16(overlay_stretch_x),
    SESSION_I16(overlay_stretch_y),
    SESSION_I16(overlay_zoom),
    SESSION_I16(overlay_crop_left),
    SESSION_I16(overlay_crop_right),
    SESSION_I16(overlay_crop_top),
    SESSION_I16(overlay_crop_bottom),
    SESSION_I16(overlay_centre_crop),
    SESSION_I16(viewport_x),
    SESSION_I16(viewport_y),
    SESSION_I16(viewport_stretch_x),
    SESSION_I16(viewport_stretch_y),
    SESSION_I16(viewport_zoom),
    SESSION_I16(crop_left),
    SESSION_I16(crop_right),
    SESSION_I16(crop_top),
    SESSION_I16(crop_bottom),
    SESSION_I16(viewport_centre_crop),
    SESSION_I16(show_playtime),
    SESSION_I16(header_visibility),
    SESSION_I16(time_display),
    SESSION_I16(crt_television),
    SESSION_I16(progress_bar),
    SESSION_I16(artwork_position),
    SESSION_I16(repeat_mode),
    SESSION_I16(shuffle),
    SESSION_I16(volume),
    SESSION_I16(sample_rate),
    SESSION_I16(audio_latency),
    SESSION_I16(audio_period),
    SESSION_I16(audio_filter),
    SESSION_I16(rate_control),
    SESSION_I16(gapless),
    SESSION_I16(crossfade),
    SESSION_I16(tracker_loop),
    SESSION_I16(fast_forward_mode),
    SESSION_I16(fast_forward_speed),
    SESSION_I16(slow_motion_mode),
    SESSION_I16(slow_motion_speed),
    SESSION_I16(thumbnail_size),
    SESSION_I16(sleep),
    SESSION_I16(idle_screensaver),
    SESSION_I16(auto_play),
    SESSION_I16(hotkey_pause),
    SESSION_I16(hotkey_save_bookmark),
    SESSION_I16(hotkey_load_bookmark),
    SESSION_I16(hotkey_seek_back),
    SESSION_I16(hotkey_seek_forward),
    SESSION_I16(hotkey_seek_back_long),
    SESSION_I16(hotkey_seek_forward_long),
    SESSION_I16(hotkey_header),
    SESSION_I16(hotkey_repeat),
    SESSION_I16(hotkey_shuffle),
    SESSION_I16(hotkey_quit),
    SESSION_I16(hotkey_fast_forward),
    SESSION_I16(hotkey_slow_motion),
};

static wasabi_video_config base_settings;
static wasabi_video_config baseline_settings;
static int16_t base_live_quality;
static int16_t base_live_buffer;
static int16_t baseline_live_quality;
static int16_t baseline_live_buffer;
static char content_path[PATH_MAX];
static char directory_path[PATH_MAX];
static int active;

static uint64_t setting_key(const char *value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) value; cursor && *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void scope_paths(const char *uri) {
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", uri ? uri : "");
    char *separator = strrchr(directory, '/');
    if (separator && separator > directory) *separator = '\0';
    snprintf(
        content_path, sizeof(content_path), WASABI_SHARE_PATH "settings/content/%016llx.conf",
        (unsigned long long) setting_key(uri)
    );
    snprintf(
        directory_path, sizeof(directory_path), WASABI_SHARE_PATH "settings/directory/%016llx.conf",
        (unsigned long long) setting_key(directory)
    );
}

static const session_field *find_field(const char *key) {
    for (size_t index = 0; index < sizeof(fields) / sizeof(fields[0]); index++)
        if (strcmp(fields[index].key, key) == 0) return &fields[index];
    return NULL;
}

static void load_scope(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[MAX_BUFFER_SIZE * 2];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *separator = strchr(line, '=');
        if (!separator) continue;
        *separator++ = '\0';
        const session_field *field = find_field(line);
        int16_t *special = strcmp(line, "live_quality") == 0  ? &config.wasabi.live_quality
                           : strcmp(line, "live_buffer") == 0 ? &config.wasabi.live_buffer
                                                              : NULL;
        if (!field && !special) continue;
        void *target = special ? (void *) special : (char *) &config.video + field->offset;
        if (field && field->kind == session_text) {
            snprintf(target, MAX_BUFFER_SIZE, "%s", separator);
            continue;
        }
        errno = 0;
        char *end = NULL;
        const long value = strtol(separator, &end, 10);
        if (errno || end == separator || *end || value < INT16_MIN || value > INT16_MAX) continue;
        *(int16_t *) target = (int16_t) value;
    }
    fclose(file);
}

static void validate_settings(void) {
#define VALIDATE(member, low, high)                                                                                    \
    do {                                                                                                               \
        if (config.video.member < (low) || config.video.member > (high)) config.video.member = base_settings.member;   \
    } while (0)
    VALIDATE(scaling_mode, 0, 5);
    VALIDATE(rotation, 0, 3);
    VALIDATE(mirrored, 0, 1);
    VALIDATE(aspect_ratio, 0, 5);
    VALIDATE(scale_multiplier, 0, 8);
    VALIDATE(texture_filter, 0, 6);
    VALIDATE(border_colour, 0, 4);
    VALIDATE(vignette_shape, 0, 5);
    VALIDATE(vignette_scaling, 0, 1);
    VALIDATE(vignette_width, 25, 200);
    VALIDATE(vignette_height, 25, 200);
    VALIDATE(vignette_offset_x, -100, 100);
    VALIDATE(vignette_offset_y, -100, 100);
    VALIDATE(vignette_softness, 0, 100);
    VALIDATE(vignette_strength, 0, 100);
    VALIDATE(vignette_colour, 0, 1);
    VALIDATE(brightness, -100, 100);
    VALIDATE(contrast, 0, 200);
    VALIDATE(saturation, 0, 200);
    VALIDATE(hue_shift, -180, 180);
    VALIDATE(gamma, 50, 200);
    VALIDATE(visualiser, 0, 9);
    VALIDATE(overlay_mode, 0, 3);
    VALIDATE(overlay_pattern, 0, 12);
    VALIDATE(overlay_opacity, 0, 100);
    VALIDATE(overlay_x, -100, 100);
    VALIDATE(overlay_y, -100, 100);
    VALIDATE(overlay_stretch_x, -100, 100);
    VALIDATE(overlay_stretch_y, -100, 100);
    VALIDATE(overlay_zoom, 25, 200);
    VALIDATE(overlay_crop_left, 0, 100);
    VALIDATE(overlay_crop_right, 0, 100);
    VALIDATE(overlay_crop_top, 0, 100);
    VALIDATE(overlay_crop_bottom, 0, 100);
    VALIDATE(overlay_centre_crop, 0, 1);
    VALIDATE(viewport_x, -100, 100);
    VALIDATE(viewport_y, -100, 100);
    VALIDATE(viewport_stretch_x, -100, 100);
    VALIDATE(viewport_stretch_y, -100, 100);
    VALIDATE(viewport_zoom, 25, 200);
    VALIDATE(crop_left, 0, 100);
    VALIDATE(crop_right, 0, 100);
    VALIDATE(crop_top, 0, 100);
    VALIDATE(crop_bottom, 0, 100);
    VALIDATE(viewport_centre_crop, 0, 1);
    VALIDATE(show_playtime, 0, 1);
    VALIDATE(header_visibility, 0, 5);
    VALIDATE(time_display, 0, 1);
    VALIDATE(crt_television, 0, 1);
    VALIDATE(progress_bar, 0, 9);
    VALIDATE(artwork_position, 0, 2);
    VALIDATE(repeat_mode, 0, 2);
    VALIDATE(shuffle, 0, 1);
    VALIDATE(volume, 0, 100);
    VALIDATE(sample_rate, 0, 2);
    VALIDATE(audio_latency, 0, 2);
    VALIDATE(audio_period, 0, 3);
    VALIDATE(audio_filter, 0, 2);
    VALIDATE(rate_control, 0, 4);
    VALIDATE(gapless, 0, 1);
    VALIDATE(crossfade, 0, 10);
    VALIDATE(tracker_loop, 0, 1);
    VALIDATE(fast_forward_mode, 0, 2);
    VALIDATE(fast_forward_speed, 0, 3);
    VALIDATE(slow_motion_mode, 0, 2);
    VALIDATE(slow_motion_speed, 0, 2);
    VALIDATE(thumbnail_size, 0, 2);
    VALIDATE(sleep, 0, 1);
    VALIDATE(idle_screensaver, 0, 1);
    VALIDATE(auto_play, 0, 1);
    if (config.wasabi.live_quality < 0 || config.wasabi.live_quality > 3)
        config.wasabi.live_quality = base_live_quality;
    if (config.wasabi.live_buffer != 4 && config.wasabi.live_buffer != 8 && config.wasabi.live_buffer != 16
        && config.wasabi.live_buffer != 32)
        config.wasabi.live_buffer = base_live_buffer;
#undef VALIDATE
    if (config.video.hotkey_fast_forward == mux_input_r3 && config.video.hotkey_slow_motion == mux_input_l3) {
        config.video.hotkey_fast_forward = mux_input_r1;
        config.video.hotkey_slow_motion = mux_input_l1;
    }
    int16_t *hotkeys[] = {
        &config.video.hotkey_pause,
        &config.video.hotkey_save_bookmark,
        &config.video.hotkey_load_bookmark,
        &config.video.hotkey_seek_back,
        &config.video.hotkey_seek_forward,
        &config.video.hotkey_seek_back_long,
        &config.video.hotkey_seek_forward_long,
        &config.video.hotkey_header,
        &config.video.hotkey_repeat,
        &config.video.hotkey_shuffle,
        &config.video.hotkey_quit,
        &config.video.hotkey_fast_forward,
        &config.video.hotkey_slow_motion
    };
    int hotkeys_valid = 1;
    for (size_t index = 0; index < sizeof(hotkeys) / sizeof(hotkeys[0]); index++) {
        if (!wasabi_hotkey_button_valid(*hotkeys[index])) hotkeys_valid = 0;
        for (size_t earlier = 0; earlier < index; earlier++)
            if (wasabi_hotkey_uses_menu((wasabi_setting) (wasabi_setting_hotkey_pause + index))
                    == wasabi_hotkey_uses_menu((wasabi_setting) (wasabi_setting_hotkey_pause + earlier))
                && *hotkeys[index] == *hotkeys[earlier])
                hotkeys_valid = 0;
    }
    if (!hotkeys_valid) {
        wasabi_hotkey_set_defaults();
    }

    char *asset_values[] = {config.video.colour_filter, config.video.shader, config.video.overlay_image};
    for (int kind = 0; kind < wasabi_asset_kind_count; kind++) {
        char *value = asset_values[kind];
        const char *leaf = strrchr(value, '/');
        if (!value[0] || (leaf && strcasecmp(leaf + 1, "none") == 0)) {
            snprintf(value, MAX_BUFFER_SIZE, "none");
            continue;
        }
        wasabi_assets_refresh((wasabi_asset_kind) kind);
        if (strcasecmp(value, "none") != 0 && wasabi_asset_find((wasabi_asset_kind) kind, value) == 0)
            snprintf(value, MAX_BUFFER_SIZE, "none");
    }
}

static int save_scope(const char *path) {
    const size_t capacity = sizeof(fields) / sizeof(fields[0]) * (MAX_BUFFER_SIZE + 48U) + 96U;
    char *output = malloc(capacity);
    if (!output) return 0;
    size_t used = 0;
    for (size_t index = 0; index < sizeof(fields) / sizeof(fields[0]); index++) {
        const session_field *field = &fields[index];
        const void *source = (const char *) &config.video + field->offset;
        const int written =
            field->kind == session_text
                ? snprintf(output + used, capacity - used, "%s=%s\n", field->key, (const char *) source)
                : snprintf(output + used, capacity - used, "%s=%d\n", field->key, (int) *(const int16_t *) source);
        if (written < 0 || (size_t) written >= capacity - used) {
            free(output);
            return 0;
        }
        used += (size_t) written;
    }
    const int written = snprintf(
        output + used, capacity - used, "live_quality=%d\nlive_buffer=%d\n", (int) config.wasabi.live_quality,
        (int) config.wasabi.live_buffer
    );
    if (written < 0 || (size_t) written >= capacity - used) {
        free(output);
        return 0;
    }
    create_directories(path, 1);
    const int result = write_text_to_file_atomic(path, CHAR, output);
    free(output);
    return result;
}

static void clear_owned_inhibit(const char *path) {
    if (read_line_int_from(path, 1) == (int) getpid()) remove(path);
}

void wasabi_session_apply_idle_policy(void) {
    if (!active) return;

    if (config.video.idle_screensaver)
        clear_owned_inhibit(IDLE_GAME_INHIBIT);
    else
        write_text_to_file(IDLE_GAME_INHIBIT, "w", INT, (int) getpid());

    if (config.video.sleep)
        clear_owned_inhibit(IDLE_SLEEP_INHIBIT);
    else
        write_text_to_file(IDLE_SLEEP_INHIBIT, "w", INT, (int) getpid());
}

int wasabi_session_begin(const char *uri) {
    base_settings = config.video;
    base_live_quality = config.wasabi.live_quality;
    base_live_buffer = config.wasabi.live_buffer;
    scope_paths(uri);
    load_scope(directory_path);
    load_scope(content_path);
    validate_settings();
    baseline_settings = config.video;
    baseline_live_quality = config.wasabi.live_quality;
    baseline_live_buffer = config.wasabi.live_buffer;
    active = 1;
    wasabi_session_apply_idle_policy();
    return 1;
}

void wasabi_session_end(void) {
    if (!active) return;
    clear_owned_inhibit(IDLE_GAME_INHIBIT);
    clear_owned_inhibit(IDLE_SLEEP_INHIBIT);
    config.video = base_settings;
    config.wasabi.live_quality = base_live_quality;
    config.wasabi.live_buffer = base_live_buffer;
    active = 0;
}

int wasabi_session_dirty(void) {
    return active
           && (memcmp(&config.video, &baseline_settings, sizeof(config.video)) != 0
               || config.wasabi.live_quality != baseline_live_quality
               || config.wasabi.live_buffer != baseline_live_buffer);
}

void wasabi_session_discard(void) {
    if (active) {
        config.video = baseline_settings;
        config.wasabi.live_quality = baseline_live_quality;
        config.wasabi.live_buffer = baseline_live_buffer;
        wasabi_session_apply_idle_policy();
    }
}

void wasabi_session_reset(void) {
    if (!active) return;
    config.video = base_settings;
    config.wasabi.live_quality = base_live_quality;
    config.wasabi.live_buffer = base_live_buffer;
    validate_settings();
    wasabi_session_apply_idle_policy();
}

int wasabi_session_save(const int choice) {
    if (!active) return 0;
    int okay = 1;
    if (choice == 0)
        okay = save_scope(content_path);
    else if (choice == 1)
        okay = save_scope(directory_path);
    else if (choice == 3) {
        wasabi_session_discard();
        return 1;
    }
    if (okay) {
        baseline_settings = config.video;
        baseline_live_quality = config.wasabi.live_quality;
        baseline_live_buffer = config.wasabi.live_buffer;
    }
    return okay;
}
